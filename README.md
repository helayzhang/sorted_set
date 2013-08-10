###SortedSet——“在C++中使用类似Redis的SortedSet数据类型”

移植Redis的z_set实现，改装成C++ template class，方便轻量的应用中不引入Redis也能使用类似其SortedSet功能的数据结构，几乎完全移植自Redis。

存储结构为Key-Score对，Key为自定义类型，Score限定为数字类型(内部实现为double).

###API列表(使用方法类似Redis Command):
zadd, zincrby, zrem, zremrangebyscore, zremrangebyrank, zrange, zrevrange, zrange_withscores, zrevrange_withscores, zrangebyscore, zrevrangebyscore, zrangebyscore_withscores, zrevrangebyscore_withscores, zcount, zcard, zscore, zrank, zrevrank
