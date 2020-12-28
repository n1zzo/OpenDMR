/***************************************************************************
 *   Copyright (C) 2020 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN                             *
 *                         Frederik Saraci IU2NRO                          *
 *                         Silvano Seva IU2KWO                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "I2C0.h"
#include <os.h>
#include <MK22F51212.h>

OS_MUTEX i2c_mutex;
OS_ERR err;

void i2c0_init()
{
    SIM->SCGC4 |= SIM_SCGC4_I2C0(1);

    I2C0->A1   = 0;                 /* Module address when in slave mode */
    I2C0->F    = 0x2C;              /* Divide bus clock by 576           */
    I2C0->C1  |= I2C_C1_IICEN(1);   /* Enable I2C module                 */

    OSMutexCreate(&i2c_mutex, "", &err);
}

void i2c0_terminate()
{
    while(I2C0->S & I2C_S_BUSY(1)) ;    /* Wait for bus free */

    I2C0->C1   &= ~I2C_C1_IICEN(1);
    SIM->SCGC4 &= ~SIM_SCGC4_I2C0(1);

    OSMutexDel(&i2c_mutex, OS_OPT_DEL_NO_PEND, &err);
}

void i2c0_write(uint8_t addr, void* buf, size_t len, bool sendStop)
{
    I2C0->C1 |= I2C_C1_TX_MASK      /* Transmit data */
             |  I2C_C1_MST_MASK;    /* Master mode   */
    I2C0->D  = addr & 0xFE;

    for(size_t i = 0; i < len; i++)
    {
        while((I2C0->S & I2C_S_IICIF_MASK) == 0) ;  /* IICIF set on tx completion */
        I2C0->S |= I2C_S_IICIF_MASK;                /* Clear IICIF flag           */
        I2C0->D = ((char *) buf)[i];
    }

    while((I2C0->S & I2C_S_IICIF_MASK) == 0) ;      /* Wait for last byte */
    I2C0->S |= I2C_S_IICIF_MASK;

    if(sendStop)
    {
        I2C0->C1 &= ~(I2C_C1_MST_MASK | I2C_C1_TX_MASK);
        while(I2C0->S & I2C_S_BUSY_MASK) ;
    }
}

void i2c0_read(uint8_t addr, void* buf, size_t len)
{
    /* In case a stop was not sent, send a repeated start instead of a start. */
    if(I2C0->C1 & I2C_C1_MST_MASK)
    {
        I2C0->C1 |= I2C_C1_RSTA_MASK
                 |  I2C_C1_TX_MASK;
    }
    else
    {
        I2C0->C1 |= I2C_C1_TX_MASK
                 |  I2C_C1_MST_MASK;
    }

    I2C0->D = addr | 0x01;

    while((I2C0->S & I2C_S_IICIF_MASK) == 0) ;  /* Wait end of address transfer   */
    I2C0->S |= I2C_S_IICIF_MASK;                /* Clear IICIF flag               */

    I2C0->C1 &= ~I2C_C1_TX_MASK;   /* Configure peripheral for data reception     */
    (void) I2C0->D;                /* Flush RX with a dummy read, also clears TCF */

    for(size_t i = 0; i < len - 1; i++)
    {
        while((I2C0->S & I2C_S_IICIF_MASK) == 0) ;
        I2C0->S |= I2C_S_IICIF_MASK;
        ((char *) buf)[i] = I2C0->D;
    }

    /* Send NACK on last byte read */
    I2C0->C1 |= I2C_C1_TXAK_MASK;

    while((I2C0->S & I2C_S_IICIF_MASK) == 0) ;
    I2C0->S |= I2C_S_IICIF_MASK;

    /* All bytes received, send stop */
    I2C0->C1 &= ~(I2C_C1_MST_MASK | I2C_C1_TXAK_MASK);

    /* Read last byte */
    ((char *) buf)[len - 1] = I2C0->D;

    /* Wait until stop has been sent */
    while(I2C0->S & I2C_S_BUSY_MASK) ;
}

bool i2c0_busy()
{
    return (I2C0->S & I2C_S_BUSY_MASK);
}

bool i2c0_lockDevice()
{
    OSMutexPend(&i2c_mutex, 0, OS_OPT_PEND_NON_BLOCKING, NULL, &err);

    if(err == OS_ERR_NONE)
    {
        return true;
    }

    return false;
}

void i2c0_lockDeviceBlocking()
{
    OSMutexPend(&i2c_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
}

void i2c0_releaseDevice()
{
    OSMutexPost(&i2c_mutex, OS_OPT_POST_NONE, &err);
}