#define GHEX_DEBUG_LEVEL 2
#include <allocator/persistent_allocator.hpp>

#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

const int SIZE = 4000000;
typedef ghex::allocator::persistent_allocator<unsigned char, std::allocator<unsigned char>> t_allocator;

t_allocator allocator;

TEST(allocator, persistent_allocator) {

    /** allocate a few arrays */
    unsigned char *p1, *p2;
    p1 = allocator.allocate(SIZE);
    p2 = allocator.allocate(SIZE);

    EXPECT_TRUE(allocator.free_alloc.size() == 0);
    EXPECT_TRUE(allocator.used_alloc.size() == 2);

    /** release and allocate again, but a smaller buffer:
     *  should use the existing buffer 
    */
    allocator.deallocate(p2, SIZE);

    EXPECT_TRUE(allocator.free_alloc.size() == 1);
    EXPECT_TRUE(allocator.used_alloc.size() == 1);

    p2 = allocator.allocate(SIZE/2);
    
    EXPECT_TRUE(allocator.free_alloc.size() == 0);
    EXPECT_TRUE(allocator.used_alloc.size() == 2);
    
    /** release and allocate again, but a larger buffer:
     *  should make a new allocation
    */
    allocator.deallocate(p1, SIZE);
    p1 = allocator.allocate(SIZE*2);

    EXPECT_TRUE(allocator.free_alloc.size() == 1);
    EXPECT_TRUE(allocator.used_alloc.size() == 2);
}
