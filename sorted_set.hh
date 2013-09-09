/*******************************************************************************
 *
 *      @file: sorted_set.hh
 *
 *      @brief: An Sorted Set, implemented by Skiplist and HashTable(std::unordered_map).
 *              We know Redis is an in-memory database that persists on disk. 
 *              We like the data type 'Sorted Set' of Redis very much, and want to
 *              port it into a cplusplus template class. So this one is, enjoy it.
 *              The API is very similar with Redis's offical command.
 *
 *      @author: Helay Zhang.
 *               helayzhang@gmail.com
 *
 *      COPYRIGHT (C) 2013.
 *
 ******************************************************************************/
#ifndef SORTEDSET_hh_INCLUDED
#define SORTEDSET_hh_INCLUDED

#include <cassert>
#include <vector>
#include <iostream>
#include <functional>

#if defined __GNUC__
#   if  __GNUC__ >= 4 && __GNUC_MINOR__ >= 3
#       include <unordered_map>
#       include <unordered_set>
#       define HASHSCOPE std
#   else
#       include <tr1/unordered_map>
#       include <tr1/unordered_set>
#       define HASHSCOPE std::tr1
#   endif
#else
#   error not GNU C Compiler
#endif

template< typename KeyType,
          typename HashFn = HASHSCOPE::hash<KeyType>,
          typename EqualKey = std::equal_to<KeyType> >
class SortedSet {
private:
    typedef typename HASHSCOPE::unordered_map<KeyType, double, HashFn, EqualKey> DictType;
    typedef typename DictType::iterator DictTypeIterator;
    typedef typename DictType::const_iterator DictTypeConstIterator;
    typedef typename std::vector<KeyType> KeyVecType;
    typedef typename KeyVecType::iterator KeyVecTypeIterator;
    typedef typename KeyVecType::const_iterator KeyVecTypeConstIterator;
    typedef typename std::pair<KeyType, double> KeyScorePairType;
    typedef typename std::vector<KeyScorePairType> KeyScoreVecType;
    typedef typename KeyScoreVecType::iterator KeyScoreVecTypeIterator;
    typedef typename KeyScoreVecType::const_iterator KeyScoreVecTypeConstIterator;
private:
    static const int SKIPLIST_MAXLEVEL = 32;
    // Fix for compile problems before C++11 compiler
    //static constexpr double SKIPLIST_P = 0.25;
    class SkipListNode;
    class SkipListLevel;
    class RangeSpec;
private:
    unsigned long length() {
        return mLength;
    }

    int randomlevel() 
    {
        int level = 1;
        while ((random()&0xFFFF) < (0.25/*SKIPLIST_P*/ * 0xFFFF))
            level += 1;
        return (level<SKIPLIST_MAXLEVEL) ? level : SKIPLIST_MAXLEVEL;
    }

    SkipListNode* private_insert(double score, KeyType key) 
    {
        SkipListNode *update[SKIPLIST_MAXLEVEL], *x;
        unsigned int rank[SKIPLIST_MAXLEVEL];
        int i, level;

        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            /* store rank that is crossed to reach the insert position */
            rank[i] = i == (mLevel-1) ? 0 : rank[i+1];
            while (x->mLevel[i].mForward && x->mLevel[i].mForward->mScore < score) {
                rank[i] += x->mLevel[i].mSpan;
                x = x->mLevel[i].mForward;
            }
            update[i] = x;
        }
        /* For skiplist self, 'key' stands
         * we assume the key is not already inside, since we allow duplicated
         * scores, and the re-insertion of score and redis object should never
         * happen since the caller of zslInsert() should test in the hash table
         * if the element is already inside or not. */
        level = randomlevel();
        if (level > mLevel) {
            for (i = mLevel; i < level; i++) {
                rank[i] = 0;
                update[i] = mHeader;
                update[i]->mLevel[i].mSpan = mLength;
            }
            mLevel = level;
        }
        x = new SkipListNode(level, score, key);
        for (i = 0; i < level; i++) {
            x->mLevel[i].mForward = update[i]->mLevel[i].mForward;
            update[i]->mLevel[i].mForward = x;
    
            /* update span covered by update[i] as x is
             * inserted here */
            x->mLevel[i].mSpan = update[i]->mLevel[i].mSpan - (rank[0] - rank[i]);
            update[i]->mLevel[i].mSpan = (rank[0] - rank[i]) + 1;
        }
    
        /* increment span for untouched levels */
        for (i = level; i < mLevel; i++) {
            update[i]->mLevel[i].mSpan++;
        }

        x->mBackward = (update[0] == mHeader) ? NULL : update[0];
        if (x->mLevel[0].mForward)
            x->mLevel[0].mForward->mBackward = x;
        else
            mTail = x;
        mLength++;
        return x;
    }

    void private_delete_node(SkipListNode *x, SkipListNode **update) 
    {
        int i;
        for (i = 0; i < mLevel; i++) {
            if (update[i]->mLevel[i].mForward == x) {
                update[i]->mLevel[i].mSpan += x->mLevel[i].mSpan - 1;
                update[i]->mLevel[i].mForward = x->mLevel[i].mForward;
            } else {
                update[i]->mLevel[i].mSpan -= 1;
            }
        }
        if (x->mLevel[0].mForward) {
            x->mLevel[0].mForward->mBackward = x->mBackward;
        } else {
            mTail = x->mBackward;
        }
        while(mLevel > 1 && mHeader->mLevel[mLevel-1].mForward == NULL)
            mLevel--;
        mLength--;
    }

    /* Delete an element with matching score/key from the skiplist. */
    bool private_delete(double score, KeyType key) 
    {
        SkipListNode *update[SKIPLIST_MAXLEVEL], *x;
        int i;
        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            while (x->mLevel[i].mForward && x->mLevel[i].mForward->mScore < score)
                x = x->mLevel[i].mForward;
            update[i] = x;
        }
        /* We may have multiple elements with the same score, what we need
         * is to find the element with both the right score and key. */
        x = x->mLevel[0].mForward;
        if (x && score == x->mScore && x->mKey == key) {
            private_delete_node(x, update);
            delete x;
            return true;
        } 
        else {
            return false; /* not found */
        }
        return false; /* not found */
    }

    static bool score_gte_min(double score, const RangeSpec &spec) {
        return spec.mMinex ? (score > spec.mMin) : (score >= spec.mMin);
    }

    static bool score_lte_max(double score, const RangeSpec &spec) {
        return spec.mMaxex ? (score < spec.mMax) : (score <= spec.mMax);
    }

    /* Returns if there is a part of the skiplist is in range. */
    bool is_in_range(const RangeSpec &range) {
        SkipListNode *x;
        /* Test for ranges that will always be empty. */
        if (range.mMin > range.mMax || (range.mMin == range.mMax && (range.mMinex || range.mMaxex)))
            return false;
        x = mTail;
        if (x == NULL || !score_gte_min(x->mScore, range))
            return false;
        x = mHeader->mLevel[0].mForward;
        if (x == NULL || !score_lte_max(x->mScore, range))
            return false;
        return true;
    }

    /* Find the first node that is contained in the specified range.
     * Returns NULL when no element is contained in the range. */
    SkipListNode* first_in_range(const RangeSpec &range) {
        SkipListNode *x;
        int i;
    
        /* If everything is out of range, return early. */
        if (!is_in_range(range)) return NULL;
    
        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            /* Go forward while *OUT* of range. */
            while (x->mLevel[i].mForward && !score_gte_min(x->mLevel[i].mForward->mScore, range))
                x = x->mLevel[i].mForward;
        }
    
        /* This is an inner range, so the next node cannot be NULL. */
        x = x->mLevel[0].mForward;
        assert(x != NULL);

        /* Check if score <= max. */
        if (!score_lte_max(x->mScore, range)) return NULL;
        return x;
    }

    /* Find the last node that is contained in the specified range.
     * Returns NULL when no element is contained in the range. */
    SkipListNode* last_in_range(const RangeSpec &range) {
        SkipListNode *x;
        int i;
    
        /* If everything is out of range, return early. */
        if (!is_in_range(range)) return NULL;
    
        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            /* Go forward while *IN* range. */
            while (x->mLevel[i].mForward && score_lte_max(x->mLevel[i].mForward->mScore, range))
                x = x->mLevel[i].mForward;
        }
    
        /* This is an inner range, so this node cannot be NULL. */
        assert(x != NULL);
    
        /* Check if score >= min. */
        if (!score_gte_min(x->mScore, range)) return NULL;
        return x;
    }

    /* Delete all the elements with score between min and max from the skiplist.
     * Min and max are inclusive, so a score >= min || score <= max is deleted.
     * Note that this function takes the reference to the hash table view of the
     * ordered set, in order to remove the elements from the hash table too. */
    unsigned long private_delete_range_by_score(const RangeSpec &range) {
        SkipListNode *update[SKIPLIST_MAXLEVEL], *x;
        unsigned long removed = 0;
        int i;
    
        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            while (x->mLevel[i].mForward && 
                (range.mMinex ? x->mLevel[i].mForward->mScore <= range.mMin 
                              : x->mLevel[i].mForward->mScore < range.mMin))
                x = x->mLevel[i].mForward;
            update[i] = x;
        }
    
        /* Current node is the last with score < or <= min. */
        x = x->mLevel[0].mForward;

        /* Delete nodes while in range. */
        while (x && (range.mMaxex ? x->mScore < range.mMax : x->mScore <= range.mMax)) {
            SkipListNode *next = x->mLevel[0].mForward;
            private_delete_node(x, update);
            mDict.erase(x->mKey);
            delete x;
            removed++;
            x = next;
        }
        return removed;
    }

    /* Delete all the elements with rank between start and end from the skiplist.
     * Start and end are inclusive. Note that start and end need to be 1-based */
    unsigned long private_delete_range_by_rank(unsigned int start, unsigned int end) {
        SkipListNode *update[SKIPLIST_MAXLEVEL], *x;
        unsigned long traversed = 0, removed = 0;
        int i;

        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            while (x->mLevel[i].mForward && (traversed + x->mLevel[i].mSpan) < start) {
                traversed += x->mLevel[i].mSpan;
                x = x->mLevel[i].mForward;
            }
            update[i] = x;
        }

        traversed++;
        x = x->mLevel[0].mForward;
        while (x && traversed <= end) {
            SkipListNode *next = x->mLevel[0].mForward;
            private_delete_node(x, update);
            mDict.erase(x->mKey);
            delete x;
            removed++;
            traversed++;
            x = next;
        }
        return removed;
    }

    /* Find the rank for an element by both score and key.
     * Returns 0 when the element cannot be found, rank otherwise.
     * Note that the rank is 1-based due to the span of mHeader to the
     * first element. */
    unsigned long get_rank(double score, KeyType key) {
        SkipListNode *x;
        unsigned long rank = 0;
        int i;

        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            while (x->mLevel[i].mForward && x->mLevel[i].mForward->mScore <= score) {
                rank += x->mLevel[i].mSpan;
                x = x->mLevel[i].mForward;
            }

            /* x might be equal to mHeader, so test if is header */
            if (not x->mIsHeader && x->mKey == key) {
                return rank;
            }
        }
        return 0;
    }

    /* Finds an element by its rank. The rank argument needs to be 1-based. */
    SkipListNode* get_element_by_rank(unsigned long rank) {
        SkipListNode *x;
        unsigned long traversed = 0;
        int i;

        x = mHeader;
        for (i = mLevel-1; i >= 0; i--) {
            while (x->mLevel[i].mForward && (traversed + x->mLevel[i].mSpan) <= rank)
            {
                traversed += x->mLevel[i].mSpan;
                x = x->mLevel[i].mForward;
            }
            if (traversed == rank) {
                return x;
            }
        }
        return NULL;
    }

    void zadd_generic(KeyType key, double score, bool incr) {
        DictTypeIterator it = mDict.find(key);
        if (it != mDict.end()) {
            double curscore = it->second;
            if (incr) {
                score += curscore;
            }
            if (score != curscore) {
                private_delete(curscore, key);
                private_insert(score, key);
                mDict[key] = score;
            }
        }
        else {
            private_insert(score, key);
            mDict[key] = score;
        }
    }
    
    void zrange_generic(long start, long end, bool reverse, KeyVecType &result) {
        result.clear();

        /* Sanitize indexes. */
        long llen = length();
        if (start < 0) start = llen+start;
        if (end < 0) end = llen+end;
        if (start < 0) start = 0;

        /* Invariant: start >= 0, so this test will be true when end < 0.
         * The range is empty when start > end or start >= length. */
        if (start > end || start >= llen) {
            return;
        }
        if (end >= llen) end = llen-1;
        unsigned long rangelen = (end-start)+1;

        SkipListNode *ln;
        /* Check if starting point is trivial, before doing log(N) lookup. */
        if (reverse) {
            ln = mTail;
            if (start > 0)
                ln = get_element_by_rank(llen-start);
        } else {
            ln = mHeader->mLevel[0].mForward;
            if (start > 0)
                ln = get_element_by_rank(start+1);
        }

        while(rangelen--) {
            assert(ln != NULL);
            result.push_back(ln->mKey);
            ln = reverse ? ln->mBackward : ln->mLevel[0].mForward;
        }
    }

    void zrange_withscores_generic(long start, long end, bool reverse, KeyScoreVecType &result) {
        result.clear();

        /* Sanitize indexes. */
        long llen = length();
        if (start < 0) start = llen+start;
        if (end < 0) end = llen+end;
        if (start < 0) start = 0;

        /* Invariant: start >= 0, so this test will be true when end < 0.
         * The range is empty when start > end or start >= length. */
        if (start > end || start >= llen) {
            return;
        }
        if (end >= llen) end = llen-1;
        unsigned long rangelen = (end-start)+1;

        SkipListNode *ln;
        /* Check if starting point is trivial, before doing log(N) lookup. */
        if (reverse) {
            ln = mTail;
            if (start > 0)
                ln = get_element_by_rank(llen-start);
        } else {
            ln = mHeader->mLevel[0].mForward;
            if (start > 0)
                ln = get_element_by_rank(start+1);
        }

        while(rangelen--) {
            assert(ln != NULL);
            result.push_back(std::make_pair(ln->mKey, ln->mScore));
            ln = reverse ? ln->mBackward : ln->mLevel[0].mForward;
        }
    }

    void zrangebyscore_generic(double min, double max, bool reverse, KeyVecType &result, 
                               bool minex = false, bool maxex = false) {
        result.clear();
        RangeSpec range((reverse?max:min), (reverse?min:max), (reverse?maxex:minex), (reverse?minex:maxex));
        SkipListNode *ln;

        /* If reversed, get the last node in range as starting point. */
        if (reverse) {
            ln = last_in_range(range);
        } else {
            ln = first_in_range(range);
        }

        /* No "first" element in the specified interval. */
        if (ln == NULL) {
            return;
        }
        while (ln) {
            /* Abort when the node is no longer in range. */
            if (reverse) {
                if (!score_gte_min(ln->mScore,range)) break;
            } else {
                if (!score_lte_max(ln->mScore,range)) break;
            }

            result.push_back(ln->mKey);

            /* Move to next node */
            if (reverse) {
                ln = ln->mBackward;
            } else {
                ln = ln->mLevel[0].mForward;
            }
        }
    }

    void zrangebyscore_withscores_generic(double min, double max, bool reverse, KeyScoreVecType &result, 
                                          bool minex = false, bool maxex = false) {
        result.clear();
        RangeSpec range((reverse?max:min), (reverse?min:max), (reverse?maxex:minex), (reverse?minex:maxex));
        SkipListNode *ln;

        /* If reversed, get the last node in range as starting point. */
        if (reverse) {
            ln = last_in_range(range);
        } else {
            ln = first_in_range(range);
        }

        /* No "first" element in the specified interval. */
        if (ln == NULL) {
            return;
        }
        while (ln) {
            /* Abort when the node is no longer in range. */
            if (reverse) {
                if (!score_gte_min(ln->mScore,range)) break;
            } else {
                if (!score_lte_max(ln->mScore,range)) break;
            }

            result.push_back(std::make_pair(ln->mKey, ln->mScore));

            /* Move to next node */
            if (reverse) {
                ln = ln->mBackward;
            } else {
                ln = ln->mLevel[0].mForward;
            }
        }
    }

    bool zrank_generic(KeyType key, bool reverse, unsigned long &rank) {
        unsigned long llen = length();
        DictTypeIterator it = mDict.find(key);
        if (it != mDict.end()) {
            double score = it->second;
            rank = get_rank(score, key);
            if (reverse) {
                rank = llen - rank;
            }
            else {
                rank -= 1;
            }
            return true;
        }
        else {
            return false;
        }
    }

public:
    void zadd(KeyType key, double score) {
        zadd_generic(key, score, false);
    }
    
    void zincrby(KeyType key, double score) {
        zadd_generic(key, score, true);
    }

    void zrem(KeyType key) {
        DictTypeIterator it = mDict.find(key);
        if (it != mDict.end()) {
            double score = it->second;
            private_delete(score, key);
            mDict.erase(it);
        }
    }

    void zremrangebyscore(double min, double max, bool minex = false, bool maxex = false) {
        RangeSpec range(min, max, minex, maxex);
        private_delete_range_by_score(range);
    }

    void zremrangebyrank(long start, long end) {
        long llen = length();
        if (start < 0) start = llen + start;
        if (end < 0) end = llen + end;
        if (start < 0) start = 0;
        /* Invariant: start >= 0, so this test will be true when end < 0.
         * The range is empty when start > end or start >= length. */
        if (start > end || start >= llen) {
            return;
        }
        if (end >= llen) end = llen - 1;
        /* Correct for 1-based rank. */
        private_delete_range_by_rank(start+1, end+1);
    }

    void zrange(long start, long end, KeyVecType &result) {
        zrange_generic(start, end, false, result);
    }
    
    void zrevrange(long start, long end, KeyVecType &result) {
        zrange_generic(start, end, true, result);
    }
    
    void zrange_withscores(long start, long end, KeyScoreVecType &result) {
        zrange_withscores_generic(start, end, false, result);
    }
    
    void zrevrange_withscores(long start, long end, KeyScoreVecType &result) {
        zrange_withscores_generic(start, end, true, result);
    }
    
    void zrangebyscore(double min, double max, KeyVecType &result, bool minex = false, bool maxex = false) {
        zrangebyscore_generic(min, max, false, result, minex, maxex);
    }
    
    void zrevrangebyscore(double min, double max, KeyVecType &result, bool minex = false, bool maxex = false) {
        zrangebyscore_generic(min, max, true, result, minex, maxex);
    }
    
    void zrangebyscore_withscores(double min, double max, KeyScoreVecType &result, bool minex = false, bool maxex = false) {
        zrangebyscore_withscores_generic(min, max, false, result, minex, maxex);
    }
    
    void zrevrangebyscore_withscores(double min, double max, KeyScoreVecType &result, bool minex = false, bool maxex = false) {
        zrangebyscore_withscores_generic(min, max, true, result, minex, maxex);
    }

    unsigned long zcount(double min, double max, bool minex = false, bool maxex = false) {
        RangeSpec range(min, max, minex, maxex);
        SkipListNode *zn;
        unsigned long rank;
        unsigned long count = 0;

        /* Find first element in range */
        zn = first_in_range(range);

        /* Use rank of first element, if any, to determine preliminary count */
        if (zn != NULL) {
            rank = get_rank(zn->mScore, zn->mKey);
            count = (mLength - (rank - 1));

            /* Find last element in range */
            zn = last_in_range(range);

            /* Use rank of last element, if any, to determine the actual count */
            if (zn != NULL) {
                rank = get_rank(zn->mScore, zn->mKey);
                count -= (mLength - rank);
            }
        }
        return count;
    }

    unsigned long zcard() {
        return length();
    }

    bool zscore(KeyType key, double &score) {
        DictTypeConstIterator it = mDict.find(key);
        if (it != mDict.end()) {
            score = it->second;
            return true;
        }
        else {
            return false;
        }
    }

    bool zrank(KeyType key, unsigned long &rank) {
        return zrank_generic(key, false, rank);
    }

    bool zrevrank(KeyType key, unsigned long &rank) {
        return zrank_generic(key, true, rank);
    }

public:
    SortedSet():mTail(NULL), mLength(0), mLevel(1), mDict()
    {
        mHeader = new SkipListNode(SKIPLIST_MAXLEVEL);
    }

    ~SortedSet() 
    {
        SkipListNode *node = mHeader->mLevel[0].mForward, *next;
        delete mHeader;
        while (node) {
            next = node->mLevel[0].mForward;
            delete node;
            node = next;
        }
    }

private:
    class SkipListNode {
    public:
        SkipListNode(int level): mIsHeader(true), mScore(0), mBackward(NULL)
        {
            mLevel = new SkipListLevel[level];
        }

        SkipListNode(int level, double score, KeyType key): mIsHeader(false), mScore(score), mKey(key), mBackward(NULL)
        {
            mLevel = new SkipListLevel[level];
        }

        ~SkipListNode()
        {
            if (mLevel) delete [] mLevel;
        }

    public:
        /* SkipListNode maybe used as a SkipList's 'header' node, which will never take a real key/score.
         * So the key/score field of a 'header' SkipListNode is always invalid, 
         * and we need this flag to judge 'header' node. */
        bool mIsHeader;
        double mScore;
        KeyType mKey;
        SkipListNode *mBackward;
        SkipListLevel *mLevel;
    };

    class SkipListLevel {
    public:
        SkipListLevel():mForward(NULL), mSpan(0) {}
        SkipListNode *mForward;
        unsigned int mSpan;
    };

    class RangeSpec {
    public:
        RangeSpec(double min, double max, bool minex, bool maxex):mMin(min), mMax(max), mMinex(minex), mMaxex(maxex) {}
    public:
        double mMin, mMax;
        bool mMinex, mMaxex;
    };

private:
    /* Data structure for the SkipList */
    SkipListNode *mHeader, *mTail;
    unsigned long mLength;
    int mLevel;
    /* Data structure for the Dict */
    DictType mDict;
};

#endif
