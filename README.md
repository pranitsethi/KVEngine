LegoBricks 
----------

a brick at a time...


Control APIs:

 bringUp(int N, int capacity, int reliabilityLevel):
o Starts a set N of Redis servers, each with a specific capacity, where capacity is the
number of key-value pairs each server can store.
o The cluster should support a given reliability level, such that it can tolerate at least
reliabilityLevel – 1 failures.
o Note that this command should be executed before any other command is executed.

 failNode(int port):
o Kill a redis server at a given port
o This can be called before and after I/O

I/O Path APIs:
 put(int key, int value): store a key-value pair on one or more of redis servers
 get(int key): retrieve the value for a given value



Write up (highlights only)  
--------

Consistent hashing:
- 

2. consistent hashing with vnodes
3. simple interface to add <K,V>
4. simple interface to retrieve <V> for a <K> (Saved <K>)


 In this case consistent hashing alone doesn’t meet our requirements of reliability due to loss of data. Therefore there should definetely be replication and high availability which is feasible and out of scope of this introduction.



Consensus (reliability level(RL)): 
-

To meet the given requirments RL must at least be equal to or less than a minimum number of copies we feel safe with. Therefore, the cluster can support RL - 1 failures. We feel that 2 copies is bare minimum and 3 copies is sufficient to provide safe reliability. Anything over
3 is overkill and less than 2 is unacceptable for reliability. 

we will place the copies in the next two or three (RL) servers in the ring.

TODO: quoroum consensum algorithm 

We will go with the simple majority quorum consensus algorithm. For this exercise a relibility level is assumed to be met if 
majority of the cluster can be satisfied. 

If a node goes down and a new one isn't added in time, then a new reliability level is calculated which will be higher(33% to 40%
for instance for 5 to 4 node clusters) resulting in same number of copies. 

When a node is added  back to the ring, then the reliability level is adjusted back, thus again resulting in the same number of copies. 

If a minimum level of reliability can't be met then the cluster goes down. 

We won't deal with moving keys back when nodes are back up or shard migration.



Questions to consider
 What happens to existing keys that no longer have predefined reliabilityLevel after failNode()
has been called?
 What should the reliabilityLevel be for puts() after failNode()
 Can your solution support a large number of nodes and a large load of requests?


