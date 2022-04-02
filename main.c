#include <sys/types.h>

#include <sys/file.h>
#include <libapi.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libsn.h>
// #include "remote.h"
#include <libsio.h>
#include <libcomb.h>
#include <string.h>
/**************************************************************************/
#define	PIH	(320)
#define	PIV	(240)
#define	OTLEN	(16)
#define	BUFSIZE		(8)
/**************************************************************************/
static	int	side;
static	long	ot[2][OTLEN];
static	DISPENV	disp[2];
static	DRAWENV	draw[2];

static long consoleplayer;
static long fr,fw;			/* file descriptor */
static unsigned long ev_w, ev_r, ev_e;	/* write/read/error event descriptor */
static unsigned char recbuf[BUFSIZE];	/* recieive buffer */
static unsigned char senbuf[BUFSIZE];	/* send buffer */
static volatile long errcnt1, errcnt2;	/* error counter */
static long wr_flg, re_flg;		/* write recovery/read recovery */
/*************************************************************************/
#define HW_U8(x) (*(volatile unsigned char *)(x))
#define HW_U16(x) (*(volatile unsigned short *)(x))
#define HW_U32(x) (*(volatile unsigned long *)(x))
#define HW_S8(x) (*(volatile signed char *)(x))
#define HW_S16(x) (*(volatile signed short *)(x))
#define HW_S32(x) (*(volatile signed long *)(x))

#define SIO1_DATA HW_U8(0x1f801050)
#define SIO1_STAT HW_U16(0x1f801054)
#define SIO1_MODE HW_U16(0x1f801058)
#define SIO1_CTRL HW_U16(0x1f80105a)
#define SIO1_BAUD HW_U16(0x1f80105e)
/*************************************************************************/


void init(void) {
    SetDispMask(0);
	ResetGraph(0);
	SetGraphDebug(0);
	ResetCallback();
	PadInit(0);
	/* initialize drawing environment & screen double buffer */
	SetDefDrawEnv(&draw[0], 0,   0, PIH, PIV);
	SetDefDrawEnv(&draw[1], 0, PIV, PIH, PIV);
	SetDefDispEnv(&disp[0], 0, PIV, PIH, PIV);
	SetDefDispEnv(&disp[1], 0,   0, PIH, PIV);
	draw[0].isbg = draw[1].isbg = 1;
	setRGB0( &draw[0], 0, 0, 64 );
	setRGB0( &draw[1], 0, 0, 64 );
	PutDispEnv(&disp[0]);
	PutDrawEnv(&draw[0]);
	side = 0;
	SetDispMask(1);
	/* Initialize onscreen font and text output system */
	FntLoad(960, 256);
	SetDumpFnt(FntOpen(16, 16, 256, 200, 0, 512));
}

void linktest() {
    while((CombSioStatus() & (COMB_DSR|COMB_CTS)) == 0x180 ) {
        if (PadRead(0) & PADselect) {
            CombCancelRead(); // Cancel async read request
            return;
        }
    }

    if(CombCTS() == 0) { /* Return status of CTS */
        consoleplayer = 0;
        read(fr, recbuf, BUFSIZE);
        do
        {
            if (PadRead(0) & PADselect)
			{
				CombCancelRead(); // Cancel async read request
				return;
			}
        } while (TestEvent(ev_r) == 0);

        //do {} while (_comb_control(3,0,0) == 0);   //Original line
        do {} while (CombCTS() == 0);   /* Return status of CTS */

        write(fw, senbuf, BUFSIZE);
    }  else {
        {
        consoleplayer = 1;
        write(fw, senbuf, BUFSIZE);
        read(fr, recbuf, BUFSIZE);
        do
        {
            if (PadRead(0) & PADselect)
			{
				CombCancelRead(); // Cancel async read request
				return;
			}
        } while (TestEvent(ev_r) == 0);
    }
    }
    if (consoleplayer == 0) {
        do {} while (CombCTS() == 0);/* Return status of CTS */
        write(fw, senbuf, BUFSIZE);
		read(fr, recbuf, BUFSIZE);
        do {} while (TestEvent(ev_r) == 0);

    }

    
}

int main(void) {
    long	hcnt;
	long	sencnt, reccnt;
    long testcnt = 0;
    long sio_stat;
    init();
    while (!(PadRead(0) & PADstart)) {

    }

    EnterCriticalSection();
    __asm__("nop");
    ExitCriticalSection();

	/* attacth the SIO driver to the kernel */
    AddCOMB();

    /* open an event to detect the end of read operation */
    ev_r = OpenEvent(HwSIO, EvSpIOER, EvMdNOINTR, NULL);
    EnableEvent(ev_r);

    /* open an event to detect the end of write operation */
    ev_w = OpenEvent(HwSIO, EvSpIOEW, EvMdNOINTR, NULL);
    EnableEvent(ev_w);

    /* open stream for writing async*/
    fw = open("sio:", O_WRONLY|O_NOWAIT);

    /* open stream for reading async */
    fr = open("sio:", O_RDONLY|O_NOWAIT);

    /* set comminucation rate */
    CombSetBPS(9600);

    
    sencnt = reccnt = 0;
    
    while(1) {
        ClearOTag( ot[side], OTLEN );
        hcnt = VSync(0);
		side ^= 1;
		PutDispEnv(&disp[side]);
		PutDrawEnv(&draw[side]);
		DrawOTag( ot[side^1] );

        if (PadRead(0) & PADstart) {
            CombReset();
        }


		sio_stat = SIO1_STAT;
		u_char txrdy = (u_char)sio_stat & 0x1;
		u_char rxfifo = ((u_char)sio_stat & 0x2) >> 1;
		u_char txrdy2 = ((u_char)sio_stat & 0x4) >> 2;
		u_char rxinput = ((u_char)sio_stat & 0x64) >> 6;
		u_char dsr = (u_char)sio_stat >> 7;
		u_char cts = ((unsigned short)sio_stat & 0x100) >> 8;




		
		FntPrint("Serial Tool\n");
		FntPrint("\nCOMBSIOSTAT: %x\n", (CombSioStatus() & (COMB_DSR|COMB_CTS)));
        	FntPrint("RTS: %d  DTR: %d\n",((CombControlStatus() & 0xFFFFFF)>>1) ,((CombControlStatus() & 1)));
        	FntPrint("CTS: %d  DSR: %d\n", CombCTS(), (CombSioStatus() >> 7) & 1);
		FntPrint("L: RTS ON R: RTS OFF\n");
        	FntPrint("U: DTR ON D: DTR OFF\n");
        	FntPrint("BYTES REMAINING READ: %d\n", CombBytesRemaining(0));
        	FntPrint("BYTES REMAINING WRITE: %d\n", CombBytesRemaining(1));
		// SIO_DATA
		FntPrint("SIO1_DATA: %x\n", SIO1_DATA);
		// SIO_STAT
		FntPrint("SIO1_STAT - TXRDY:%d TXRDY2:%d\nRXFIFO:%d RXINPUT:%d DSR:%d CTS:%d",
			txrdy, txrdy2, rxfifo, rxinput, dsr, cts);

		
		






		FntFlush(-1);
	    	
        	// if (_rec_remote()==1 ) reccnt++;
        	// if (_send_remote()==1) sencnt++;


        // long remain = CombBytesRemaining(fr);
		// // dsr_on = (CombSioStatus() & 0xFFFF) >> 7;
		// // cts_on = (CombSioStatus() & 0xFFFF1) >> 8;
		// long comm_rate = _comb_control(0,3,0);
		// long comm_mode = _comb_control(0,2,0);
		// long unit_num = _comb_control(0,4,0);
		// // long remain = _comb_control(0,5,0);
		// u_char cts = _comb_control(3,1,0);
		// long con_stat = _comb_control(0,0,0);
		// u_char con_line = _comb_control(0,1,0);
		// u_char rts, dtr, dsr;
		// // if (con_line == 1) {
		// // 	rts = 1;
		// // } else if (con_line == 0) {
		// // 	dtr = 1;
		// // } else {
		// // 	rts = 0;
		// // 	dtr = 0;
		// // }
		// char cls = CombControlStatus();
		// if ((cls >> 1) == 1) {
		// 	rts = 1;
		// } else {
		// 	rts = 0;
		// }
		// if ((cls & 1) == 1) {
		// 	dtr = 1;
		// } else {
		// 	dtr = 0;
		// }
		

		// cts_on = CombCTS();
		// if(CombCTS() == 0) {
		// 	read(fr, recbuf, BUFSIZE);
		// 	// write(fw, recbuf, BUFSIZE);
		// }
		// // read(fr, recbuf, BUFSIZE);
		// if (TestEvent(ev_r) == 1) reccnt++;
		// if (TestEvent(ev_w) == 1) sencnt++;
		// dsr = (con_stat & 0xffff) >> 6;
		
		// FntPrint("\nREMOTE CONTROLLER\n\n");
		// FntPrint("ANSYNCRONOUS READ\n");
		// FntPrint("ANSYNCRONOUS WRITE\n\n");
		
        
        // FntPrint("\nSEND     %d    RECEIVE  %d\n", sencnt, reccnt );
		// FntPrint("ERROR1:  %d      ERROR2: %d\n", _get_error_count1(), _get_error_count2());
		// FntPrint("\nCOMBSIOSTAT: %x\n", (CombSioStatus() & (COMB_DSR|COMB_CTS)));
        // FntPrint("RTS: %d  DTR: %d\n",((CombControlStatus() & 0xFFFFFF)>>1) ,((CombControlStatus() & 1)));
        // FntPrint("CTS: %d  DSR: %d\n", CombCTS(), (CombSioStatus() >> 7));
		// FntPrint("L: RTS ON R: RTS OFF\n");
        // FntPrint("U: DTR ON D: DTR OFF\n");
        // FntPrint("\n BYTES REMAINING: %d\n", CombBytesRemaining(0));
        // FntPrint("\n BYTES REMAINING: %d\n", CombBytesRemaining(1));
        // FntPrint("\n PLAYER: %d\n", consoleplayer);
        // FntPrint("\nBUFFER:\n");
        // FntPrint("SIO1_DATA: %x", SIO1_DATA);
        // if(recbuf) {
        //     FntPrint("%s", recbuf);
        //     // write( fw, recbuf, 10 );
        //     // FntPrint("\n\nSending\n");
        // }
        // FntPrint("%s", senbuf);
        if ((PadRead(0) & PADselect)) {
            CombReset();
            memset(recbuf, 0, 64);
        }
        if (PadRead(0) & PADLleft) {
            CombSetRTS(1);
        } else if (PadRead(0) & PADLright) {
            CombSetRTS(0);
        }
        if (PadRead(0) & PADLup) {
            CombSetControl(1);
        } else if (PadRead(0) & PADLdown) {
            CombSetControl(0);
        }
        if (PadRead(0) & PADRdown) {
        }
        // FntPrint("\n RTS STATUS: %d", rts);
		// FntPrint("\n CTS STATUS: %d", cts_on);
		// FntPrint("\n COM STAT: %d", con_stat);
		// FntPrint("\n DTR STATUS: %d", dtr);
		// FntPrint("\n BAUD: %d", comm_rate);
		// FntPrint("\n MODE: %d", comm_mode);
		// FntPrint("\n UNITS: %d", unit_num);
		// FntPrint("\n BYTES REMAINING: %d\n", remain);
		// FntPrint("\n RESETS: %d\n", rsts);
		 
		
		// if( recbuf ){
        //     FntPrint("%s", recbuf);
        // }
        // if( strlen(recbuf) > BUFSIZE ) {
        //        // If that limit is reached, remove first char in string
        //        memmove(recbuf, recbuf + 1, strlen(recbuf));
        //     }


        // linktest();

		
    }
    return 0;
}
