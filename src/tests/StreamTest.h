/* StreamTest.h
 *
 */
#ifndef STREAMTEST_H
#define STREAMTEST_H 1

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "Stream.h"

class StreamTest : public CPPUNIT_NS :: TestFixture
{
    CPPUNIT_TEST_SUITE (StreamTest);
    CPPUNIT_TEST (reserveTest);
    CPPUNIT_TEST (tellTest);
    CPPUNIT_TEST (seekTest);
    CPPUNIT_TEST (putsTest);
    CPPUNIT_TEST (truncateTest);
    CPPUNIT_TEST (getsTest);
    CPPUNIT_TEST (printfTest);
    CPPUNIT_TEST_SUITE_END ();

    public:
        void setUp (void);
        void tearDown (void);

    protected:
        void reserveTest(void);
        void tellTest(void);
        void seekTest(void);
        void putsTest(void);
        void truncateTest(void);
        void getsTest(void);
        void printfTest(void);
    private:
        Stream *a, *b, *c, *d, *e, *f, *g, *h;
	char cstring[50];
};
#endif

