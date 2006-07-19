/* EncryptedStreamTest.c
 *
 */
#include "EncryptedStreamTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (EncryptedStreamTest);

void EncryptedStreamTest :: setUp (void)
{
    strcpy(cstring, "Some static cstring to play with");
    // set up test environment (initializing objects)
    a = new EncryptedStream(NULL);
    b = new EncryptedStream("blah");
    c = new EncryptedStream(*b);
}

void EncryptedStreamTest :: tearDown (void)
{
    // finally delete objects
    delete a; delete b; delete c; 
}

void EncryptedStreamTest :: getsTest (void)
{
return;
  char buf[51];

  a->printf("This is line 1\n");
  a->printf("This is line 2\n");
  a->printf("This is line 3\n");
  a->printf("This is line 4\n");
  a->printf("This is line 5\n");
  a->printf("This is line 6\n");
  a->printf("This is line 7\n");
  a->printf("This is line 8\n");
  a->printf("This is line 9\n");
  a->printf("This is line10\n");
  a->printf("This is line11\n");
  a->printf("This is line12\n");
  a->printf("This is line13\n");
  a->seek(0, SEEK_SET);
//std::cout << *a << std::endl;
  int x = 0;
  size_t pos = 0;
  while (a->tell() < a->length()) {
    ++x;
    pos += a->gets(buf, sizeof(buf));
    CPPUNIT_ASSERT_EQUAL(pos, a->tell());
    CPPUNIT_ASSERT_EQUAL((size_t) 15, strlen(buf));
  }
  CPPUNIT_ASSERT_EQUAL(13, x);
}

void EncryptedStreamTest :: printfTest (void)
{
  char buf[51];

  /* a has no key set. */
  a->printf("This is line 1\n");
  a->printf("This is line 2\n");
  a->printf("This is line 3\n");
  a->printf("This is line 4\n");
  a->printf("This is line 5\n");
  a->printf("This is line 6\n");
  a->printf("This is line 7\n");
  a->printf("This is line 8\n");
  a->printf("This is line 9\n");
  a->printf("This is line10\n");
  a->printf("This is line11\n");
  a->printf("This is line12\n");
  a->printf("This is line13\n");
  a->seek(0, SEEK_SET);
  int x = 0;
  size_t pos = 0, size = 0;
  while (a->tell() < a->length()) {
    ++x;
    size = a->gets(buf, sizeof(buf));
    pos += size;
    CPPUNIT_ASSERT_EQUAL(pos, a->tell());
    CPPUNIT_ASSERT_EQUAL((size_t) 15, strlen(buf));
    CPPUNIT_ASSERT_EQUAL((size_t) 15, size);
  }
  CPPUNIT_ASSERT_EQUAL(13, x);

  /* b does */
  b->printf("This is line 1\n");
  b->printf("This is line 2\n");
  b->printf("This is line 3\n");
  b->printf("This is line 4\n");
  b->printf("This is line 5\n");
  b->printf("This is line 6\n");
  b->printf("This is line 7\n");
  b->printf("This is line 8\n");
  b->printf("This is line 9\n");
  b->printf("This is line10\n");
  b->printf("This is line11\n");
  b->printf("This is line12\n");
  b->printf("This is line13\n");
  b->seek(0, SEEK_SET);
  x = 0;
  pos = 0;
  size = 0;
  while (b->tell() < b->length()) {
    ++x;
    size = b->gets(buf, sizeof(buf));
    CPPUNIT_ASSERT_EQUAL((size_t) 15, strlen(buf));
    CPPUNIT_ASSERT_EQUAL((size_t) 15, size);
  }
  CPPUNIT_ASSERT_EQUAL(13, x);
}
