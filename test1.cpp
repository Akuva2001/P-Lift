#include <set>
#include <iostream>
#include <cstdio>
using namespace std;

int main1(){
    set<pair<int, int>> myset;
    myset.insert(pair<int, int>(1, 2));
    myset.insert(pair<int, int>(1, 3));
    auto lb = myset.lower_bound(pair<int,int>(8, 2));
    if (lb == myset.end()) printf("scream1\n");
    auto ub = myset.upper_bound(pair<int,int>(8, 2));
    if (ub == myset.end()) printf("scream2\n");
    return 0;
}

int main(){
    set<int> myset;
    myset.insert(1);
    myset.insert(3);
    auto lb = myset.lower_bound(8);
    if (lb == myset.end()) printf("scream1\n");
    auto ub = myset.upper_bound(8);
    if (ub == myset.end()) printf("scream2\n");
    return 0;
}
