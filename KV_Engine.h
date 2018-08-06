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

class reply { 


public: 

 enum class type_t
    {
        STRING = 1,
        ARRAY = 2,
        INTEGER = 3,
        NIL = 4,
        STATUS = 5,
        ERROR = 6
    };
   
    inline type_t type() const { return _type; }
    inline const std::string& str() const { return _str; }


    reply(redisReply* reply) {}
    reply();

    type_t _type;
    std::string _str;
    long long _integer;
    std::vector<reply> _elements;

    friend class connection;

};

class connection
{

  enum State { 
      OK = 0, 
      ERROR = 1,
      DOWN  = 2, 
  };  

  const static int TotalCapacity = 100; // tunable
public:
    connection* ptr_t;
    State value;
    int port; // hash seed
    int capacity; 
    int getCapacity() { return capacity; }
    int getPort() { return port; }
    bool underCapacity() const; 
    void incrCapacity() { ++capacity; }
    int hash; // hash value for this server
    list<int> vserver;
   

    /**
     * @brief Create and open a new connection
     * @param host hostname or ip of redis server, default localhost
     * @param port port of redis server, default: 6379
     * @return
     */
    static connection* create(const std::string& host="localhost",
                               const unsigned int port=6379)
    {
        return (new connection(host, port));
    }

    /**
     * @brief Create and open a new connection
     * @param path to unix socket
     * @return
     */
    connection* create_unix(const std::string& path)
    {
        return (new connection(path));
    }

    ~connection();

    bool is_valid() const;

    /**
     * @brief Append a command to Redis server
     * @param args vector with args, example [ "SET", "foo", "bar" ]
     */
    void append(const std::vector<std::string>& args);

    /**
     * @brief Get a reply from server, blocking call if no reply is ready
     * @return reply object
     */
    reply get_reply();

    /**
     * @brief Get specific count of replies requested, blocking if they
     * are not ready yet.
     * @param count
     * @return
     */
    std::vector<reply> get_replies(unsigned int count);

    /**
     * @brief Utility to call append and then get_reply together
     * @param args same as {@link append()}
     * @return reply object
     */
    inline reply run(const std::vector<std::string>& args)
    {
        append(args);
        return get_reply();
    }

    /**
     * @brief Returns raw ptr to hiredis library connection.
     * Use it with caution and pay attention on memory
     * management.
     * @return
     */
    inline redisContext* c_ptr() { return c; }

    enum role_t {
        ANY = 0,
        MASTER = 1,
        SLAVE = 2
    };

private:
    connection(const std::string& host, const unsigned int port);
    connection(const std::string& path);

    role_t _role;
    redisContext *c;
};

class Ring { 

  const int virtNodes = 2; // arbitrary choice of two extra vnodes per a physical node
  int TotalNodes;

public: 
  map<int, connection*> ring_map;
  int  addNodeToRing(connection*); // connection represents a server
  void removeNode(connection*); // marks it down 
  void reAddNode(connection*);  // could readjust RL
  int hash(int N) { return N; } // elementary hash method
  connection* getNode(int hash);
  connection* getNextNode(int hash);

  Ring(): TotalNodes(0) {}; 

};


class KeyValue_Engine { 


 // this class runs the overall engine
 // bringup:
 //    start servers
 //    build connections to each server
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
      int  addNodeToRing(connection* c) {ring_engine.addNodeToRing(c);}
      void failNode(int port); // removes the node and vnodes from the CH ring
      int  put(int key, int value);
      int  get(int key);
      int  startServers(int N);
      int  setRL(int N, int requested_RL);
      int  getRL();
};




