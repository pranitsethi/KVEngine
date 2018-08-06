//#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <hiredis/hiredis.h>
#include <map>
#include <list>
#include <unistd.h>

struct redisContext;
struct redisReply;

using namespace std;

class redserver
{

  enum State { 
      OK = 0, 
      ERROR = 1,
      DOWN  = 2, 
  };  

  const static int TotalCapacity = 100; // tunable
public:
    redserver* ptr_t;
    State value;
    int port; // hash seed
    int capacity; 
    int getCapacity() { return capacity; }
    int getPort() { return port; }
    bool underCapacity() const; 
    void incrCapacity() { ++capacity; }
    int hash; // hash value for this server
    list<int> vserver;
    void addToList(int hash) { vserver.push_back(hash); }
   

    /**
     * @brief Create and open a new redserver
     * @param host hostname or ip of redis server, default localhost
     * @param port port of redis server, default: 6379
     * @return
     */
    static redserver* create(const std::string& host="localhost",
                               const unsigned int port=6379)
    {
        return (new redserver(host, port));
    }

    ~redserver();

    inline redisContext* c_ptr() { return c; }

private:
    redserver(const std::string& host, const unsigned int port);
    redserver(const std::string& path);

    redisContext *c;
};

class Ring { 

  const int virtNodes = 2; // arbitrary choice of two extra vnodes per a physical node
  int TotalNodes;

public:
  int stride; 
  map<int, redserver*> ring_map;
  int  addNodeToRing(redserver*); // redserver represents a server
  void removeNode(redserver*); // marks it down 
  void reAddNode(redserver*);  // could readjust RL
  int hash(int N) { return N; } // elementary hash method
  redserver* getNode(int hash);
  redserver* getNextNode(int hash);
  redserver* getFailedNode(int hash);
  void        removeVnodes(redserver*);
  void        removePnode(redserver*);

  Ring(): TotalNodes(0), stride(10) {}; 

};


class KeyValue_Engine { 


 // this class runs the overall engine
 // bringup:
 //    start servers
 //    build redservers to each server
 //    place the servers and vnodes in a consistent hash ring



   const string str = "127.0.0.1"; 
   const unsigned port = 9012;  // servers are hashed onto the ring per their port# (since IP's are same) 
   const unsigned min_RL = 2; 
   const unsigned max_RL = 3;

   int numServers;
   int capacity; // upper bound on each server

   int reliabilityLevel; // initial supplied RL
   int curr_RL; // current reliability level
   int min_copies; 
   int max_copies; 

   // ring
   Ring ring_engine;


 public:
     
      KeyValue_Engine(int N, int capacity, int reliabilityLevel);
      int  bringUp(int N, int capacity, int reliabiltyLevel);
      int  addNodeToRing(redserver* c) {ring_engine.addNodeToRing(c);}
      void failNode(int port); // removes the node and vnodes from the CH ring
      int  put(int key, int value);
      int  get(int key);
      int  startServers(int N);
      int  setRL(int N, int requested_RL);
      int  getRL();
      void shutdownEngine() {}  // TODO
};




