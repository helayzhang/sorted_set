###Redis SortedSet for C++

Redis's SortedSet is POWERFUL tool, an awesome Data-Structure to organizing data for sorting, hashing and ranking. See [Redis SortedSet](http://www.redis.io/topics/data-types#sorted-sets).

With the C++ client of redis, C++ programers can use its power easily. 

Can we achieve these ranking features without Redis?
The answer is YES! One easiest way is combining STL's `<map>` and `<unordered_map>`, then creates your "Sorted Set", which can get values by O(1) complexity, and get ranks by O(log(N)).

However, redis use skiplist, which has a better performance than STL's red-black tree. So I have ported the redis sortedset source code into a stand alone C++ template, for any feature use.

####HOW TO USE: 
The score type is always **DOUBLE**, so you only need to indicate a key type. All APIs has a very similar interfaces with origin Redis commands, you can refer to the C++ header and the Redis command documents for quick startup. Refer [Redis SortedSet Commands](http://www.redis.io/commands#sorted_set).

Examples:

    SortedSet<int> sortedSet;     
    sortedSet.zadd(1, 300);
    sortedSet.zadd(2, 299.9);
    sortedSet.zadd(3, 100000);
    
    std::vector< std::pair<int, double> > result;
    sortedSet.zrange_withscores(0, -1, result);
    std::for_each(result.begin(), result.end(), echo_ranking);
    
    sortedSet.zrem(3);
    sortedSet.zincrby(3, 100);
    sortedSet.zrank(3);

####NOTE:
I implement it with STL's `<unordered_map>`, which is included in "std::tr1" namespace in older compilers. If you use gcc/g++ compilers, there is no worry for you, otherwise you may have to fix the namespace problem and define your one to "HASHSCOPE". Like this:

`#define HASHSCOPE std` or `#define HASHSCOPE std::tr1`