#include <hid/ps2mouse.h>
#include <asm/bootpack.h>
#include <hid/keyboard.h>
#include <sys/pic.h>

#include <ostype.h>
#include <ioqueue.h>

#include <sys/int.h>
#include <protect.h>


CircleQueue mouse_queue;
struct mouse_data{
    int_32 x;
    int_32 y;
    int_32 btn;
};

struct mouse_raw_data {
    uint_8 buf[3];
    uint_8 stage; // mouse decoding process
    struct mouse_data cooked_mdata;
};

int mouse_decode(struct mouse_raw_data *mdata, uint_8 code);
void inthandler2C(void);

void enable_mouse(){
    wait_KBC_sendready();
    _io_out8(PORT_KEYCOMMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    _io_out8(PORT_KEYDATE, MOUSECMD_ENABLE);
    register_r0_intr_handler(INT_VECTOR_PS2_MOUSE, inthandler2C);
    return;
}

int mouse_decode(struct mouse_raw_data *mdata, uint_8 code){
    if(mdata->stage == 0){
        if(code == 0xfa){
            mdata->stage = 1;
        }
        return 0;
    } else if (mdata->stage == 1){
        if ((code & 0xc8) == 0x08){
            mdata->buf[0] = code;
            mdata->stage = 2;
        }
        return 0;
    } else if (mdata->stage == 2){
        mdata->buf[1] = code;
        mdata->stage = 3;
        return 0;
    } else if (mdata->stage == 3){
        mdata->buf[2] = code;
        mdata->stage = 1;
        mdata->cooked_mdata.btn = mdata->buf[0] & 0x07;
        mdata->cooked_mdata.x = mdata->buf[1];
        mdata->cooked_mdata.y = mdata->buf[2];
        return 1;
    }
    return -1;
}

/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
void inthandler2C(void){
    _io_out8(PIC1_OCW2, PIC_EOI_IRQ12); // tell slave  IRQ12 is finish
    _io_out8(PIC0_OCW2, PIC_EOI_IRQ2); // tell master IRQ2 is finish

    char code = _io_in8(PORT_KEYDATE) ;
//TODO: Need upgrade ioqueue to general type, it need heap stack memory 
    /* struct mouse_raw_data mrd = {0}; */
    /* int sucs = mouse_decode(&mrd, code); */
 
    /* if(sucs){ */
        struct queue_data qdata = {
            .data = code,
        };
        ioqueue_put_data(&qdata, &mouse_queue);
    /* } */

    return;
}

