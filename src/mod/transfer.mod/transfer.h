/*
 * transfer.h -- part of transfer.mod
 *
 */

#ifndef _EGG_MOD_TRANSFER_TRANSFER_H
#define _EGG_MOD_TRANSFER_TRANSFER_H

enum dccsend_types {
  DCCSEND_OK = 0,
  DCCSEND_FULL,		/* DCC table is full			*/
  DCCSEND_NOSOCK,	/* Can not open a listening socket	*/
  DCCSEND_BADFN,	/* No such file				*/
  DCCSEND_FEMPTY	/* File is empty			*/
};

enum {                          /* transfer connection handling a ...   */
        XFER_SEND,              /*  ... normal file-send to s.o.        */
        XFER_RESEND,            /*  ... file-resend to s.o.             */
        XFER_RESEND_PEND,       /*  ... (as above) and waiting for info */
        XFER_RESUME,            /*  ... file-send-resume to s.o.        */
        XFER_RESUME_PEND,       /*  ... (as above) and waiting for conn */
        XFER_GET                /*  ... file-get from s.o.              */
};

enum {
        XFER_ACK_UNKNOWN,       /* We don't know how blocks are acked.  */
        XFER_ACK_WITH_OFFSET,   /* Skipped data is also counted as
                                   received.                            */
        XFER_ACK_WITHOUT_OFFSET /* Skipped data is NOT counted in ack.  */
};

int raw_dcc_send(char *, char *, char *, int *);

#ifdef MAKING_TRANSFER
#define TRANSFER_REGET_PACKETID 0xfeab

typedef struct {
  u_32bit_t byte_offset;	/* Number of bytes to skip relative to
				   the file beginning			*/
  u_16bit_t packet_id;		/* Identification ID, should be equal
	 			   to TRANSFER_REGET_PACKETID		*/
  u_8bit_t  byte_order;		/* Byte ordering, see byte_order_test()	*/
} transfer_reget;
#endif				/* MAKING_TRANSFER */

/* Language file additions */

#define TRANSFER_COPY_FAILED        "Refused dcc get %s: copy to %s FAILED!"
#define TRANSFER_FILESYS_BROKEN     "NOTICE %s :File system is broken; aborting queued files.\n"
#define TRANSFER_FILE_ARRIVE        "NOTICE %s :Here is a file from %s ...\n"
#define TRANSFER_LOG_CONFULL        "DCC connections full: GET %s [%s]"
#define TRANSFER_NOTICE_CONFULL     "NOTICE %s :DCC connections full; aborting queued files.\n"
#define TRANSFER_LOG_SOCKERR        "DCC socket error: GET %s [%s]"
#define TRANSFER_NOTICE_SOCKERR     "NOTICE %s :DCC socket error; aborting queued files.\n"
#define TRANSFER_LOG_FILEEMPTY      "Aborted dcc get %s: File is empty!"
#define TRANSFER_NOTICE_FILEEMPTY   "NOTICE %s :File %s is empty, aborting transfer.\n"
#define TRANSFER_SEND_TO            "  Send to  %s  Filename\n"
#define TRANSFER_LINES              "  ---------%s  --------------------\n"
#define TRANSFER_WAITING            "  %s%s  %s  [WAITING]\n"
#define TRANSFER_DONE               "  %s%s  %s  (%.1f%% done)\n"
#define TRANSFER_QUEUED_UP          "No files queued up.\n"
#define TRANSFER_TOTAL              "Total: %d\n"
#define TRANSFER_CANCELLED          "Cancelled: %s to %s\n"
#define TRANSFER_ABORT_DCCSEND      "Cancelled: %s  (aborted dcc send)\n"
#define TRANSFER_NOTICE_ABORT       "NOTICE %s :Transfer of %s aborted by %s\n"
#define TRANSFER_DCC_CANCEL         "DCC cancel: GET %s (%s) at %lu/%lu"
#define TRANSFER_NO_MATCHES         "No matches.\n"
#define TRANSFER_CANCELLED_FILE     "Cancelled %d file%s.\n"
#define TRANSFER_COMPLETED_DCC      "Completed dcc send %s from %s!%s"
#define TRANSFER_FILENAME_TOOLONG   "Filename %d length. Way To LONG."
#define TRANSFER_NOTICE_FNTOOLONG   "NOTICE %s :Filename %d length Way To LONG!\n"
#define TRANSFER_TOO_BAD            "Too Bad So Sad Your Dad!"
#define TRANSFER_NOTICE_TOOBAD      "NOTICE %s :Too Bad So Sad Your Dad!\n"
#define TRANSFER_FAILED_MOVE        "FAILED move `%s' from `%s'! File lost!"
#define TRANSFER_THANKS             "Thanks for the file!\n"
#define TRANSFER_NOTICE_THANKS      "NOTICE %s :Thanks for the file!\n"
/* #define TRANSFER_BYE	            "0xf1e) */
#define TRANSFER_LOST_DCCSEND       "Lost dcc send %s from %s!%s (%lu/%lu)"
#define TRANSFER_RESUME_FILE        "Resuming file transfer at %dk for %s to %s"
#define TRANSFER_FINISHED_DCCSEND   "Finished dcc send %s to %s"
#define TRANSFER_LOST_DCCGET        "Lost dcc get %s from %s!%s"
#define TRANSFER_NOTICE_TIMEOUT     "NOTICE %s :Timeout during transfer, aborting %s.\n"
#define TRANSFER_LOG_TIMEOUT        "EGGDROP TEAM IS TEH GAY"
#define TRANSFER_DCC_GET_TIMEOUT    "DCC timeout: GET %s (%s) at %lu/%lu"
#define TRANSFER_DCC_SEND_TIMEOUT   "DCC timeout: SEND %s (%s) at %lu/%lu"
#define TRANSFER_BEGIN_DCC          "Begin DCC %ssend %s to %s"
#define TRANSFER_RE                 "re"
#define TRANSFER_DCC_IGNORED        "NOTICE %s :Ignoring resume of `%s': no data requested.\n"
#define TRANSFER_UNLOADING          "Unloading transfer module, killing all transfer connections..."
#define TRANSFER_STAT_MEMORY        "   Using %d bytes of memory\n"
/* end of langauge addon */

extern struct dcc_table 		DCC_SEND, DCC_FORK_SEND;
extern struct dcc_table			DCC_GET, DCC_GET_PENDING;

#endif				/* _EGG_MOD_TRANSFER_TRANSFER_H */
