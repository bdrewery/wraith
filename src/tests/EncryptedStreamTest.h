/* EncryptedStreamTest.h
 *
 */
#ifndef EncryptedStreamTest_H
#define EncryptedStreamTest_H 1

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "EncryptedStream.h"

class EncryptedStreamTest : public CPPUNIT_NS :: TestFixture
{
    CPPUNIT_TEST_SUITE (EncryptedStreamTest);
    CPPUNIT_TEST (getsTest);
    CPPUNIT_TEST (printfTest);
    CPPUNIT_TEST_SUITE_END ();

    public:
        void setUp (void);
        void tearDown (void);

    protected:
        void getsTest(void);
        void printfTest(void);
    private:
        EncryptedStream *a, *b, *c, *d, *e, *f, *g, *h;
	char cstring[50];
};
#endif

