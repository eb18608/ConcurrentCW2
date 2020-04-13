/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

// Proc Tab declaration
pcb_t procTab[ MAX_PROCS ]; pcb_t* executing = NULL;

// Memory allocation addressing for processes

// Base address for processses in image.ld
extern uint32_t tos_P;
// Addressing the main_functions of user processes
extern void     main_P1();
extern void     main_P2();
extern void     main_P3();
extern void     main_P4();
extern void     main_P5();
extern void     main_P6();



void (*prog[MAX_PROCS])()={
  main_P1, main_P2, main_P3, main_P4, main_P5, main_P6
};




// Dispatch function for process switching
void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );

    executing = next;                           // update   executing process to P_{next}

  return;
}


void scheduleSVC( ctx_t* ctx  ) {
    if     ( executing->pid == procTab[ 0 ].pid ) {
      dispatch( ctx, &procTab[ 0 ], &procTab[ 1 ] );  // context switch P_1 -> P_2
  
      procTab[ 0 ].status = STATUS_READY;             // update   execution status  of P_2
      procTab[ 1 ].status = STATUS_EXECUTING;         // update   execution status  of P_1
    }
    else if ( executing->pid == procTab[ 1 ].pid){
      dispatch( ctx, &procTab[ 1 ], &procTab[ 2 ] );  // context switch P_2 -> P_1
  
      procTab[ 1 ].status = STATUS_READY;             // update   execution status  of P_1
      procTab[ 2 ].status = STATUS_EXECUTING;         // update   execution status  of P_2
    }
    else if ( executing->pid == procTab[ 2 ].pid){
      dispatch( ctx, &procTab[ 2 ], &procTab[ 3 ]);   // context switch P_3 to P_1

      procTab[ 2 ].status = STATUS_READY;             // update execution status P_3
      procTab[ 3 ].status = STATUS_EXECUTING;         // update execution status P_1
    }
    else if ( executing->pid == procTab[ 3 ].pid){
      dispatch( ctx, &procTab[ 3 ], &procTab[ 4 ]);   // context switch P_3 to P_1

      procTab[ 3 ].status = STATUS_READY;             // update execution status P_3
      procTab[ 4 ].status = STATUS_EXECUTING;         // update execution status P_1
    }
    else if ( executing->pid == procTab[ 4 ].pid){
      dispatch( ctx, &procTab[ 4 ], &procTab[ 5 ]);   // context switch P_3 to P_1

      procTab[ 4 ].status = STATUS_READY;             // update execution status P_3
      procTab[ 5 ].status = STATUS_EXECUTING;         // update execution status P_1
    }
    else if ( executing->pid == procTab[ 5 ].pid){
      dispatch( ctx, &procTab[ 5 ], &procTab[ 0 ]);   // context switch P_3 to P_1

      procTab[ 5 ].status = STATUS_READY;             // update execution status P_3
      procTab[ 0 ].status = STATUS_EXECUTING;         // update execution status P_1
    }

  return;
}

void hilevel_handler_rst( ctx_t* ctx ) {
        PL011_putc( UART0, 'R', true );
  /* Configure the mechanism for interrupt handling by
   *
   * - configuring timer st. it raises a (periodic) interrupt for each 
   *   timer tick,
   * - configuring GIC st. the selected interrupts are forwarded to the 
   *   processor via the IRQ interrupt signal, then
   * - enabling IRQ interrupts.
   */

  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();


    /* Invalidate all entries in the process table, so it's clear they are not
   * representing valid (i.e., active) processes.
   */

  for( int i = 0; i < MAX_PROCS; i++ ) {
    procTab[ i ].status = STATUS_INVALID;
  }

  /* Automatically execute the user programs P1 and P2 by setting the fields
   * in two associated PCBs.  Note in each case that
   *
   * - the CPSR value of 0x50 means the processor is switched into USR mode,
   *   with IRQ interrupts enabled, and
   * - the PC and SP values match the entry point and top of stack.
   */

  // int numProg = sizeof(prog)/ sizeof(prog[0]);

   memset( &procTab[ 0 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
   procTab[ 0 ].pid      = 1;
   procTab[ 0 ].status   = STATUS_READY;
   procTab[ 0 ].tos      = ( uint32_t )( &tos_P  );
   procTab[ 0 ].ctx.cpsr = 0x50;
   procTab[ 0 ].ctx.pc   = ( uint32_t )( &main_P1 );
   procTab[ 0 ].ctx.sp   = procTab[ 0 ].tos;
   procTab[ 0 ].calls    = 0;

  
   memset( &procTab[ 1 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
   procTab[ 1 ].pid      = 2;
   procTab[ 1 ].status   = STATUS_READY;
   procTab[ 1 ].tos      = ( uint32_t )( &tos_P + 1*0x00001000);
   procTab[ 1 ].ctx.cpsr = 0x50;
   procTab[ 1 ].ctx.pc   = ( uint32_t )( &main_P2 );
   procTab[ 1 ].ctx.sp   = procTab[ 1 ].tos;
   procTab[ 1 ].calls    = 0;

   memset( &procTab[ 2 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
   procTab[ 2 ].pid      = 3;
   procTab[ 2 ].status   = STATUS_READY;
   procTab[ 2 ].tos      = ( uint32_t )( &tos_P + 2*0x00001000);
   procTab[ 2 ].ctx.cpsr = 0x50;
   procTab[ 2 ].ctx.pc   = ( uint32_t )( &main_P3 );
   procTab[ 2 ].ctx.sp   = procTab[ 2 ].tos;
   procTab[ 2 ].calls    = 0;

   memset( &procTab[ 3 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
   procTab[ 3 ].pid      = 4;
   procTab[ 3 ].status   = STATUS_READY;
   procTab[ 3 ].tos      = ( uint32_t )( &tos_P + 3*0x00001000 );
   procTab[ 3 ].ctx.cpsr = 0x50;
   procTab[ 3 ].ctx.pc   = ( uint32_t )( &main_P4 );
   procTab[ 3 ].ctx.sp   = procTab[ 3 ].tos;
   procTab[ 3 ].calls    = 0;

   memset( &procTab[ 4 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
   procTab[ 4 ].pid      = 5;
   procTab[ 4 ].status   = STATUS_READY;
   procTab[ 4 ].tos      = ( uint32_t )( &tos_P + 4*0x00001000 );
   procTab[ 4 ].ctx.cpsr = 0x50;
   procTab[ 4 ].ctx.pc   = ( uint32_t )( &main_P5 );
   procTab[ 4 ].ctx.sp   = procTab[ 4 ].tos;
   procTab[ 4 ].calls    = 0;

   memset( &procTab[ 5 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
   procTab[ 5 ].pid      = 6;
   procTab[ 5 ].status   = STATUS_READY;
   procTab[ 5 ].tos      = ( uint32_t )( &tos_P + 5*0x00001000 );
   procTab[ 5 ].ctx.cpsr = 0x50;
   procTab[ 5 ].ctx.pc   = ( uint32_t )( &main_P6 );
   procTab[ 5 ].ctx.sp   = procTab[ 5 ].tos;
   procTab[ 5 ].calls    = 0;  

  /* Once the PCBs are initialised, we arbitrarily select the 0-th PCB to be
   * executed: there is no need to preserve the execution context, since it
   * is invalid on reset (i.e., no process was previously executing).
   */

  dispatch( ctx, NULL, &procTab[ 0 ] );

  return;
}

void hilevel_handler_irq( ctx_t* ctx ) {

  // Step 2: read  the interrupt identifier so we know the source.
  uint32_t id = GICC0->IAR;
  int p = executing->pid;
  bool interrupt = false;
  // Step 4: handle the interrupt, then clear (or reset) the source.
  //         Interrupt now just calls scheduleSVC
  if( id == GIC_SOURCE_TIMER0 ) {
    scheduleSVC( ctx );

    TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t svid ) {
    switch( svid ) {
      
      case 0x00 : { //0x01 => yield();
        scheduleSVC( ctx );

        break;
        }

      case 0x01 : { // 0x01 => write( fd, x, n )
        int   fd = ( int   )( ctx->gpr[ 0 ] );
        char*  x = ( char* )( ctx->gpr[ 1 ] );
        int    n = ( int   )( ctx->gpr[ 2 ] );

        for( int i = 0; i < n; i++ ) {
          PL011_putc( UART0, *x++, true );
        }
        
        ctx->gpr[ 0 ] = n;
        break;
        }

  default   : { // 0x?? => unknown/unsupported
    break;
  }
  
  }
  return;
}
