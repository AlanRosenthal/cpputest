#include "CppUTest/TestHarness.h"
#include "CppUTest/TestMemoryAllocator.h"
#include "CppUTest/MemoryLeakDetector.h"
#include "CppUTest/TestOutput.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTest/PlatformSpecificFunctions.h"
#include "CppUTest/TestTestingFixture.h"
#include "AllocationInCppFile.h"

#include "CppUTest/TestHarness_c.h"
#include "AllocationInCFile.h"

#if defined(__GNUC__)
# if __GNUC__ >= 11
#  define NEEDS_DISABLE_FREE_NON_HEEP_WARNING
# endif /* GCC >= 11 */
# if __GNUC__ >= 12
#  define NEEDS_DISABLE_USE_AFTER_FREE
# endif /* GCC >= 12 */
#endif /* GCC */


TEST_GROUP(BasicBehavior)
{
};

TEST(BasicBehavior, CanDeleteNullPointers)
{
    delete (char*) NULLPTR;
    delete [] (char*) NULLPTR;
}

#if CPPUTEST_USE_MEM_LEAK_DETECTION

#ifdef NEEDS_DISABLE_USE_AFTER_FREE
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wuse-after-free"
#endif /* NEEDS_DISABLE_USE_AFTER_FREE */

CPPUTEST_DO_NOT_SANITIZE_ADDRESS
static void deleteArrayInvalidatesMemory()
{
    unsigned char* memory = new unsigned char[10];
    PlatformSpecificMemset(memory, 0xAB, 10);
    delete [] memory;
    CHECK(memory[5] != 0xCB);
}

CPPUTEST_DO_NOT_SANITIZE_ADDRESS
static void deleteInvalidatesMemory()
{
    unsigned char* memory = new unsigned char;
    *memory = 0xAD;
    delete memory;
    CHECK(*memory != 0xAD);
}

#ifdef NEEDS_DISABLE_USE_AFTER_FREE
# pragma GCC diagnostic pop
#endif /* NEEDS_DISABLE_USE_AFTER_FREE */

TEST(BasicBehavior, deleteArrayInvalidatesMemory)
{
    deleteArrayInvalidatesMemory();
}

TEST(BasicBehavior, deleteInvalidatesMemory)
{
    deleteInvalidatesMemory();
}

#if __cplusplus >= 201402L
TEST(BasicBehavior, DeleteWithSizeParameterWorks)
{
    char* charMemory = new char;
    char* charArrayMemory = new char[10];
    ::operator delete(charMemory, sizeof(char));
    ::operator delete[](charArrayMemory, sizeof(char)* 10);
}
#endif

#ifdef NEEDS_DISABLE_FREE_NON_HEEP_WARNING
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif /* NEEDS_DISABLE_FREE_NON_HEEP_WARNING */

static void deleteUnallocatedMemory()
{
    delete (char*) 0x1234678;
    FAIL("Should never come here"); // LCOV_EXCL_LINE
} // LCOV_EXCL_LINE

#ifdef NEEDS_DISABLE_FREE_NON_HEEP_WARNING
# pragma GCC diagnostic pop
#endif /* NEEDS_DISABLE_FREE_NON_HEEP_WARNING */


TEST(BasicBehavior, deleteWillNotThrowAnExceptionWhenDeletingUnallocatedMemoryButCanStillCauseTestFailures)
{
    /*
     * Test failure might cause an exception. But according to C++ standard, you aren't allowed
     * to throw exceptions in the delete function. If you do that, it will call std::terminate.
     * Therefore, the delete will need to fail without exceptions.
     */
    MemoryLeakFailure* defaultReporter = MemoryLeakWarningPlugin::getGlobalFailureReporter();
    TestTestingFixture fixture;
    fixture.setTestFunction(deleteUnallocatedMemory);
    fixture.runAllTests();
    LONGS_EQUAL(1, fixture.getFailureCount());
    POINTERS_EQUAL(defaultReporter, MemoryLeakWarningPlugin::getGlobalFailureReporter());
}

#endif

#ifdef CPPUTEST_USE_MALLOC_MACROS

/* This include is added because *sometimes* the cstdlib does an #undef. This should have been prevented */
#if CPPUTEST_USE_STD_CPP_LIB
#include <cstdlib>
#endif

TEST(BasicBehavior, bothMallocAndFreeAreOverloaded)
{
    void* memory = cpputest_malloc_location(sizeof(char), "file", 10);
    free(memory);

    memory = malloc(sizeof(unsigned char));
    cpputest_free_location(memory, "file", 10);
}

#endif

#if CPPUTEST_USE_MEM_LEAK_DETECTION

CPPUTEST_DO_NOT_SANITIZE_ADDRESS
static void freeInvalidatesMemory()
{
    unsigned char* memory = (unsigned char*) cpputest_malloc(sizeof(unsigned char));
    *memory = 0xAD;
    cpputest_free(memory);
    CHECK(*memory != 0xAD);
}

TEST(BasicBehavior, freeInvalidatesMemory)
{
    freeInvalidatesMemory();
}
#endif

TEST_GROUP(MemoryLeakOverridesToBeUsedInProductionCode)
{
    MemoryLeakDetector* memLeakDetector;
    void setup() _override
    {
        memLeakDetector = MemoryLeakWarningPlugin::getGlobalDetector();
    }

};

#if CPPUTEST_USE_MEM_LEAK_DETECTION

#ifdef CPPUTEST_USE_NEW_MACROS
    #undef new
#endif
TEST(MemoryLeakOverridesToBeUsedInProductionCode, newDeleteOverloadsWithIntLineWorks)
{
    const int line = 42;
    char* leak = new("TestFile.cpp", line) char;

    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    STRCMP_NOCASE_CONTAINS("Allocated at: TestFile.cpp and line: 42.", memLeakDetector->report(mem_leak_period_checking));

    ::operator delete (leak, "TestFile.cpp", line);

    LONGS_EQUAL(memLeaks-1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, newDeleteOverloadsWithSizeTLineWorks)
{
    const size_t line = 42;
    char* leak = new("TestFile.cpp", line) char;

    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    STRCMP_NOCASE_CONTAINS("Allocated at: TestFile.cpp and line: 42.", memLeakDetector->report(mem_leak_period_checking));

    ::operator delete (leak, "TestFile.cpp", line);

    LONGS_EQUAL(memLeaks-1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, newDeleteArrayOverloadsWithIntLineWorks)
{
    const int line = 42;
    char* leak = new("TestFile.cpp", line) char[10];

    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    STRCMP_NOCASE_CONTAINS("Allocated at: TestFile.cpp and line: 42.", memLeakDetector->report(mem_leak_period_checking));

    ::operator delete [] (leak, "TestFile.cpp", line);

    LONGS_EQUAL(memLeaks-1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, newDeleteArrayOverloadsWithSizeTLineWorks)
{
    const size_t line = 42;
    char* leak = new("TestFile.cpp", line) char[10];

    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    STRCMP_NOCASE_CONTAINS("Allocated at: TestFile.cpp and line: 42.", memLeakDetector->report(mem_leak_period_checking));

    ::operator delete [] (leak, "TestFile.cpp", line);

    LONGS_EQUAL(memLeaks-1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
}
#ifdef CPPUTEST_USE_NEW_MACROS
    #include "CppUTest/MemoryLeakDetectorNewMacros.h" // redefine the 'new' macro
#endif
#endif

#ifdef CPPUTEST_USE_MALLOC_MACROS

TEST(MemoryLeakOverridesToBeUsedInProductionCode, MallocOverrideIsUsed)
{
    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    void* memory = malloc(10);
    LONGS_EQUAL(memLeaks+1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
    free (memory);
}

#ifdef CPPUTEST_USE_STRDUP_MACROS

TEST(MemoryLeakOverridesToBeUsedInProductionCode, StrdupOverrideIsUsed)
{
    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    char* memory = strdup("0123456789");
    LONGS_EQUAL(memLeaks+1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
    free (memory);
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, StrndupOverrideIsUsed)
{
    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
    char* memory = strndup("0123456789", 10);
    LONGS_EQUAL(memLeaks+1, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
    free (memory);
}
#endif

#endif

TEST(MemoryLeakOverridesToBeUsedInProductionCode, UseNativeMallocByTemporarlySwitchingOffMalloc)
{
    size_t memLeaks = memLeakDetector->totalMemoryLeaks(mem_leak_period_checking);
#ifdef CPPUTEST_USE_MALLOC_MACROS
    #undef malloc
    #undef free
#endif

#if CPPUTEST_USE_STD_C_LIB
    void* memory = malloc(10);
    LONGS_EQUAL(memLeaks, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
    free (memory);
#else
    void* memory = PlatformSpecificMalloc(10);
    LONGS_EQUAL(memLeaks, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
    PlatformSpecificFree (memory);
#endif

#ifdef CPPUTEST_USE_MALLOC_MACROS
#include "CppUTest/MemoryLeakDetectorMallocMacros.h"
#endif
}

/* TEST... allowing for a new overload in a class */
class NewDummyClass
{
public:
    static bool overloaded_new_called;

#ifdef CPPUTEST_USE_NEW_MACROS
    #undef new
#endif
    void* operator new (size_t size)
#ifdef CPPUTEST_USE_NEW_MACROS
    #include "CppUTest/MemoryLeakDetectorNewMacros.h"
#endif
    {
        overloaded_new_called = true;
        return malloc(size);
    }
    void dummyFunction()
    {
        char* memory = new char;
        delete memory;
    }
};
bool NewDummyClass::overloaded_new_called = false;

TEST(MemoryLeakOverridesToBeUsedInProductionCode, NoSideEffectsFromTurningOffNewMacros)
{
    /*
     * Interesting effect of wrapping the operator new around the macro is
     * that the actual new that is called is a different one than expected.
     *
     * The overloaded operator new doesn't actually ever get called.
     *
     * This might come as a surprise, so it is important to realize!
     */
    NewDummyClass dummy;
    dummy.dummyFunction();
    // CHECK(dummy.overloaded_new_called);
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, UseNativeNewByTemporarlySwitchingOffNew)
{
#ifdef CPPUTEST_USE_NEW_MACROS
    #undef new
    #undef delete
#endif
    char* memory = new char[10];
    delete [] memory;
#ifdef CPPUTEST_USE_NEW_MACROS
    #include "CppUTest/MemoryLeakDetectorNewMacros.h"
#endif
}


#if CPPUTEST_USE_MEM_LEAK_DETECTION

TEST(MemoryLeakOverridesToBeUsedInProductionCode, OperatorNewMacroOverloadViaIncludeFileWorks)
{
    char* leak = newAllocation();
    STRCMP_NOCASE_CONTAINS("AllocationInCppFile.cpp", memLeakDetector->report(mem_leak_period_checking));
    delete leak;
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, OperatorNewArrayMacroOverloadViaIncludeFileWorks)
{
    char* leak = newArrayAllocation();
    STRCMP_NOCASE_CONTAINS("AllocationInCppFile.cpp", memLeakDetector->report(mem_leak_period_checking));
    delete[] leak;
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, MallocOverrideWorks)
{
    char* leak = mallocAllocation();
    STRCMP_NOCASE_CONTAINS("AllocationInCFile.c", memLeakDetector->report(mem_leak_period_checking));
    freeAllocation(leak);
}

#ifdef CPPUTEST_USE_STRDUP_MACROS
TEST(MemoryLeakOverridesToBeUsedInProductionCode, StrdupOverrideWorks)
{
    char* leak = strdupAllocation();
    STRCMP_NOCASE_CONTAINS("AllocationInCFile.c", memLeakDetector->report(mem_leak_period_checking));
    freeAllocation(leak);
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, StrndupOverrideWorks)
{
    char* leak = strndupAllocation();
    STRCMP_NOCASE_CONTAINS("AllocationInCFile.c", memLeakDetector->report(mem_leak_period_checking));
    freeAllocation(leak);
}
#endif

TEST(MemoryLeakOverridesToBeUsedInProductionCode, MallocWithButFreeWithoutLeakDetectionDoesntCrash)
{
    char* leak = mallocAllocation();
    freeAllocationWithoutMacro(leak);
    LONGS_EQUAL(2, memLeakDetector->totalMemoryLeaks(mem_leak_period_checking));
    memLeakDetector->removeMemoryLeakInformationWithoutCheckingOrDeallocatingTheMemoryButDeallocatingTheAccountInformation(getCurrentMallocAllocator(), leak, true);
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, OperatorNewOverloadingWithoutMacroWorks)
{
    char* leak = newAllocationWithoutMacro();
    STRCMP_CONTAINS("unknown", memLeakDetector->report(mem_leak_period_checking));
    delete leak;
}

TEST(MemoryLeakOverridesToBeUsedInProductionCode, OperatorNewArrayOverloadingWithoutMacroWorks)
{
    char* leak = newArrayAllocationWithoutMacro();
    STRCMP_CONTAINS("unknown", memLeakDetector->report(mem_leak_period_checking));
    delete[] leak;
}

#else

TEST(MemoryLeakOverridesToBeUsedInProductionCode, MemoryOverridesAreDisabled)
{
    char* leak = newAllocation();
    STRCMP_EQUAL("No memory leaks were detected.", memLeakDetector->report(mem_leak_period_checking));
    delete leak;
}

#endif

TEST_GROUP(OutOfMemoryTestsForOperatorNew)
{
    TestMemoryAllocator* no_memory_allocator;
    GlobalMemoryAllocatorStash memoryAllocatorStash;
    void setup() _override
    {
        memoryAllocatorStash.save();
        no_memory_allocator = new NullUnknownAllocator;
        setCurrentNewAllocator(no_memory_allocator);
        setCurrentNewArrayAllocator(no_memory_allocator);
    }

    void teardown() _override
    {
        memoryAllocatorStash.restore();
        delete no_memory_allocator;
    }
};

#if CPPUTEST_USE_MEM_LEAK_DETECTION

#if CPPUTEST_HAVE_EXCEPTIONS

TEST(OutOfMemoryTestsForOperatorNew, FailingNewOperatorThrowsAnExceptionWhenUsingStdCppNew)
{
    CHECK_THROWS(CPPUTEST_BAD_ALLOC, new char);
}

TEST(OutOfMemoryTestsForOperatorNew, FailingNewArrayOperatorThrowsAnExceptionWhenUsingStdCppNew)
{
    CHECK_THROWS(CPPUTEST_BAD_ALLOC, new char[10]);
}

TEST_GROUP(TestForExceptionsInConstructor)
{
};

TEST(TestForExceptionsInConstructor,ConstructorThrowsAnException)
{
    CHECK_THROWS(int, new ClassThatThrowsAnExceptionInTheConstructor);
}

TEST(TestForExceptionsInConstructor,ConstructorThrowsAnExceptionAllocatedAsArray)
{
    CHECK_THROWS(int, new ClassThatThrowsAnExceptionInTheConstructor[10]);
}

#else

TEST(OutOfMemoryTestsForOperatorNew, FailingNewOperatorReturnsNull)
{
    POINTERS_EQUAL(NULLPTR, new char);
}

TEST(OutOfMemoryTestsForOperatorNew, FailingNewArrayOperatorReturnsNull)
{
    POINTERS_EQUAL(NULLPTR, new char[10]);
}

#endif

#undef new

#if CPPUTEST_HAVE_EXCEPTIONS


/*
 * CLang 4.2 and memory allocation.
 *
 * Clang 4.2 has done some optimizations to their memory management that actually causes slightly different behavior than what the C++ Standard defines.
 * Usually this is not a problem... but in this case, it is a problem.
 *
 * More information about the optimization can be found at: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3433.html
 * We've done a bug-report to clang to fix some of this non-standard behavior, which is open at: http://llvm.org/bugs/show_bug.cgi?id=15541
 *
 * I very much hope that nobody would actually ever hit this bug/optimization as it is hard to figure out what is going on.
 *
 * The original test simply did "new char". Because the memory wasn't assigned to anything and is local in context, the optimization *doesn't* call
 * the operator new overload. Because it doesn't call the operator new (optimizing away a call to operator new), therefore the method wouldn't throw an exception
 * and therefore this test failed.
 *
 * The first attempt to fix this is to create a local variable and assigned the memory to that. Also this doesn't work as it still detects the allocation is
 * local and optimizes away the memory call.
 *
 * Now, we assign the memory on some static global which fools the optimizer to believe that it isn't local and it stops optimizing the operator new call.
 *
 * We (Bas Vodde and Terry Yin) suspect that in a real product, you wouldn't be able to detect the optimization and it's breaking of Standard C++. Therefore,
 * for now, we keep this hack in the test to fool the optimizer and hope nobody will ever notice this 'optimizer behavior' in a real product.
 *
 * Update 2020: The gcc compiler implemented the same optimization, but it seems to be slightly smarter and discovered that we assign to a static variable.
 * Thus it still optimized away the call to operator new. Did another bug report, but it is unlikely to get fixed. You can find it at:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94671
 *
 * Changed the variable to be external so it would definitively be a mistake to optimize the call.
 *
 */

extern char* some_memory;
char* some_memory;

TEST(OutOfMemoryTestsForOperatorNew, FailingNewOperatorThrowsAnExceptionWhenUsingStdCppNewWithoutOverride)
{
    CHECK_THROWS(CPPUTEST_BAD_ALLOC, some_memory = new char);
}

TEST(OutOfMemoryTestsForOperatorNew, FailingNewArrayOperatorThrowsAnExceptionWhenUsingStdCppNewWithoutOverride)
{
    CHECK_THROWS(CPPUTEST_BAD_ALLOC, some_memory = new char[10]);
}

#if CPPUTEST_USE_STD_CPP_LIB
TEST(OutOfMemoryTestsForOperatorNew, FailingNewOperatorReturnsNullWithoutOverride)
{
    POINTERS_EQUAL(NULLPTR, new (std::nothrow) char);
}

TEST(OutOfMemoryTestsForOperatorNew, FailingNewArrayOperatorReturnsNullWithoutOverride)
{
    POINTERS_EQUAL(NULLPTR, new (std::nothrow) char[10]);
}
#endif

#else

TEST(OutOfMemoryTestsForOperatorNew, FailingNewOperatorReturnsNullWithoutOverride)
{
    POINTERS_EQUAL(NULLPTR, new char);
}

TEST(OutOfMemoryTestsForOperatorNew, FailingNewArrayOperatorReturnsNullWithoutOverride)
{
    POINTERS_EQUAL(NULLPTR, new char[10]);
}

#endif

#endif
