#include <KV_Engine.h>

redserver::redserver(const std::string& host, const unsigned aport) : capacity(0), port(aport)
{
    c = redisConnect(host.c_str(), aport);
    if (c->err != REDIS_OK)
    {
        redisFree(c);
        printf("connect failed, err %d\n", c->err);
    }
}

redserver::redserver(const std::string& path)
{
    c = redisConnectUnix(path.c_str());
    if (c->err != REDIS_OK)
    {
        redisFree(c);
    }
}

redserver::~redserver()
{
    redisFree(c);
}


bool redserver::underCapacity() const 
{ 
   printf("this node: %p, cap %d, totcap %d\n", this, capacity, TotalCapacity);
   return capacity < redserver::TotalCapacity;  
}

void Ring::removeVnodes(redserver* con) 
{ 

     list<int>::iterator it;
     for (it = con->vserver.begin(); it != con->vserver.end() ; ++it) { 
       
          ring_map.erase(*it);
     } 
}
  
void Ring::removePnode(redserver* con) 
{ 

          ring_map.erase(con->hash);
}  

int Ring::addNodeToRing(redserver* con) 
{

     // get the port (seed hash value)
      int ihash = hash(con->port * stride); 
      con->hash = ihash;
      ring_map[ihash]  = con; 
      printf("adding node server %p, port %d\n", con, con->getPort());
      ++TotalNodes;

      // add virtual nodes
      for (int i = 2; i < virtNodes + 2; ++i) {
           int ihash = hash(con->port * i * stride); // TODO: better spread 
           con->hash = ihash;
           ring_map[ihash]  = con; 
           con->addToList(ihash); 
           printf("adding vnode server %p, port %d\n", con, ihash);
           ++TotalNodes;
           stride *= 2; // TODO: ring hash spread  
      }

     return 0; // TODO: error handling
     
}

redserver* Ring::getNode(int hash) { 

    // given a hash; get the next server in the ring
    // if the server is down then get next and so on

    map<int, redserver*>::iterator itnode;

    printf("get: looking for hash %d\n", hash); // TODO: REMOVE
    for (itnode = ring_map.begin(); itnode != ring_map.end(); ++itnode) { 
    
         printf("get: found node %p, hash %d\n", itnode->second, itnode->first);
    } // TODO: REMOVE

    itnode = ring_map.upper_bound(hash);
    printf("getUser: found node %p\n", itnode->second);
    return itnode->second;
}

redserver* Ring::getFailedNode(int hash) { 

    // given a hash; get the next server in the ring
    // if the server is down then get next and so on

    map<int, redserver*>::iterator itnode;
    itnode = ring_map.lower_bound(hash);
    printf("getFailed: found node %p\n", itnode->second);
    return itnode->second;
}

redserver* Ring::getNextNode(int hash) { 

    // given a hash; get the next server in the ring
    // if the server is down then get next and so on

    map<int, redserver*>::iterator itnode;
    itnode = ring_map.upper_bound(hash);
    int lhash = ((redserver*)itnode->second)->hash + 1; 
    itnode = ring_map.upper_bound(lhash);
    return itnode->second;
}

void KeyValue_Engine::failNode(int port) { 

     // pull all servers (incuding vnodes) out of the ring

     int ihash = ring_engine.hash(port);
     redserver* con = (redserver*)ring_engine.getFailedNode(ihash); 
     printf("put: got back node server %p, port %d\n", con, con->getPort());
     ring_engine.removeVnodes(con);
     ring_engine.removePnode(con);
} 

int KeyValue_Engine::put(int key, int value) { 

     // hash the key 
     // find the right node (check curr_RL)
     // add the key (to replica copies subject to RL)

     int ihash = ring_engine.hash(key);

     redserver* con = (redserver*)ring_engine.getNode(ihash); 
     printf("put: got back node server %p, port %d\n", con, con->getPort());

     // check for capacity 
     if (!con->underCapacity()) { 
         printf("Primary Capacity exceeded on server %p, port %d\n", con, con->getPort());
         return -1; // TODO: error: bad hash method or overall capacity?
     } 

     redisContext* ctx = con->c_ptr();
     redisReply* r = (redisReply*)redisCommand(ctx, "SET foo bar");
     con->incrCapacity();
     printf("KV SET: %s\n", r->str);
     freeReplyObject(r);

    for (int i = 0; i < curr_RL; ++i) { 

        // make curr_RL number of copies to meet RL
        redserver* con = ring_engine.getNextNode(ihash); 
       // check for capacity 
        if (!con->underCapacity()) { 
         printf("Capacity exceeded on server %p, port %d\n", con, con->getPort());
         return -1; // TODO: error: bad hash method or overall capacity?
        } 

        redisContext* ctx = con->c_ptr();
        redisReply* r = (redisReply*)redisCommand(ctx, "SET foo bar");
        con->incrCapacity();
        printf("KV SET: %s\n", r->str);
        freeReplyObject(r);
    } 
   
   return 0; // OK 

} 


int KeyValue_Engine::get(int key) { 

     // hash the key 
     // find the right node (check for node state)
     // get the key 
     int ihash = ring_engine.hash(key);

     redserver* con = ring_engine.getNode(ihash); 
     redisContext* ctx = con->c_ptr();
     redisReply* r = (redisReply*)redisCommand(ctx, "GET foo"); 
     printf("KV reply contains %s\n", r->str);
     freeReplyObject(r);

} 

int KeyValue_Engine::startServers(int N) { 

    int curr_port  = port; 
    for (int i = 0; i < N; ++i, ++curr_port) { 

      char buf[32];
      sprintf(buf, "redis-server --port %d --daemonize yes", curr_port);
      printf("starting a server on port %d\n", curr_port); 
      system(buf);
      //system("redis-server --port 6379 --daemonize yes");
      sleep(2); // TODO: race when starting and establishing a redserver
      redserver* c  = redserver::create("127.0.0.1", curr_port);
      printf("add node to ring %d\n", curr_port); 
      addNodeToRing(c);
    }

}

int KeyValue_Engine::setRL(int N, int requested_RL) { 

  // this method is critical (and requires more thought) as it decides # of copies 
  // for each copy of data and where the copies should be placed (which is outside the 
  // scope here. However, strategically placing the copies determined a lot about the overall
  // distributed performance of the system

  // therefore we make the RL dynamic.

  // we will cap off the RL (reliability level) at 3 since we think 
  // more than 3 copies is an overkill (although we will provide a way to control 
  // it via a parameter to change it -- max_copies)

  // also there will be a minimum number of copies that must be made under any
  // circumstance. Another parameter is provided (min_copies)

  // reAddNode can adjust RL

  if (N < min_RL) 
    return -1; // no support for less than min_RL
 

  if (N >= max_RL) { 

      if (requested_RL > max_RL) {  
         // TODO: a force param set by the user to exceed max_RL
         // otherwise don't make more than max_RL copies (Default)
        curr_RL = max_RL; 

      } else { 

        curr_RL = requested_RL; 
      }    
   }
  return curr_RL;
}

int KeyValue_Engine::getRL() { 

    return curr_RL; 
 
   //if (curr_RL == reliabilityLevel) { 
    // no node has gone down for instance
   // }

}

int KeyValue_Engine::bringUp(int N, int cap, int rLevel) { 

  // start servers 
  // build redservers to each server 
  // place the servers and vnodes in a consistent hash ring 
  startServers(N);
  printf("set RL\n");
  setRL(N, rLevel);
}


KeyValue_Engine::KeyValue_Engine(int N, int cap, int rLevel): numServers(N), 
                                capacity(cap), reliabilityLevel(rLevel), min_copies(min_RL),
                                max_copies(max_RL), curr_RL(rLevel) { 

    bringUp(N, cap, rLevel);  // TODO: pull it outside (separate API)

}

       


int main(int argc, char **argv) {

   // TODO: 
   // failed node
   // pnode to vnode list



   printf("Simple test 1: starting a redis server, and adding a key\n");

   // test1
   int N = 3; 
   int cap = 10; 
   int RL = 2; 
   KeyValue_Engine kve(N, cap, RL);
   kve.put(10, 20); 
   kve.get(10);
   
   //system("redis-server --port 6379 --daemonize yes");
   //sleep(2);
   //system("redis-server --port 6380&");
   //system("redis-server --port 6381&");
   //redserver* c  = redserver::create("127.0.0.1", 6379);
   //redserver* c1  = redserver::create("127.0.0.1", 6380);
   //redserver* c2  = redserver::create("127.0.0.1", 6381);
    //redisContext* ctx = c->c_ptr();
    //redisReply* r = (redisReply*)redisCommand(ctx, "SET foo bar");
    //printf("SET: %s\n", r->str);
   //if (r->str == REDIS_REPLY_ERROR) {
    //   printf("Error return in Simple test 1: starting a redis server, and adding a key\n");
     //  return -1; 
   //} 
    //freeReplyObject(r);

    //r = (redisReply*)redisCommand(ctx, "GET foo"); 
    //printf("reply contains %s\n", r->str);
    //freeReplyObject(r);
    //delete c;
   //delete c1;
   //delete c2;*/

}
