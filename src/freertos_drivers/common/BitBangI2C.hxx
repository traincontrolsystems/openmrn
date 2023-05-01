/** @copyright
 * Copyright (c) 2023, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file BitBangI2C.hxx
 * This file implements a bit bang implementation of I2C.
 *
 * @author Stuart W. Baker
 * @date 28 March 2023
 */

#ifndef _FREERTOS_DRIVERS_COMMON_BITBANG_I2C_HXX_
#define _FREERTOS_DRIVERS_COMMON_BITBANG_I2C_HXX_

#include "I2C.hxx"
#include "os/Gpio.hxx"
#include "os/OS.hxx"

/// Implement a bit-banged I2C master. A periodic timer [interrupt] should call
/// the BitBangI2C::tick_interrupt() method at a rate that is one half the
/// desired clock rate. For example, for a 100kHz bus, call once every 5
/// microseconds. The tick should be enabled to start. The driver will
/// enable/disable the tick as needed to save on spurious interrupts.
class BitBangI2C : public I2C
{
public:
    /// Constructor.
    /// @param name name of this device instance in the file system
    /// @param sda_gpio Gpio instance of the SDA line
    /// @param scl_gpio Gpio instance of the SCL line
    /// @param enable_tick callback to enable ticks
    /// @param disable_tick callback to disable ticks
    BitBangI2C(const char *name, const Gpio *sda_gpio, const Gpio *scl_gpio,
               void (*enable_tick)(), void (*disable_tick)())
        : I2C(name)
        , sda_(sda_gpio)
        , scl_(scl_gpio)
        , enableTick_(enable_tick)
        , disableTick_(disable_tick)
        , msg_(nullptr)
        , sem_()
        , count_(0)
        , state_(State::STOP) // start-up with a stop sequence
        , stateStop_(StateStop::SDA_CLR)
        , stop_(true)
        , clockStretchActive_(false)
    {
        gpio_set(sda_);
        gpio_clr(scl_);
    }

    /// Destructor.
    ~BitBangI2C()
    {
    }

    /// Called at a periodic tick, when enabled.
    inline void tick_interrupt();

private:
    /// High level I2C States
    enum class State : uint8_t
    {
        START, ///< start state
        ADDRESS, ///< address state
        DATA_TX, ///< data TX state
        DATA_RX, ///< data RX state
        STOP, ///< stop state
    };

    /// Low level I2C start states
    enum class StateStart : uint8_t
    {
        SDA_SET, ///< start sequence
        SCL_SET, ///< start sequence
        SDA_CLR, ///< start sequence
        FIRST = SDA_SET, ///< first start sequence state
        LAST = SDA_CLR, /// last start sequence state
    };

    /// Low level I2C stop states
    enum class StateStop : uint8_t
    {
        SDA_CLR, ///< stop sequence
        SCL_SET, ///< stop sequence
        SDA_SET, ///< stop sequence
        FIRST = SDA_CLR, ///< first stop sequence state
        LAST = SDA_SET, ///< last stop sequence state
    };

    /// Low level I2C data TX states
    enum class StateTx : uint8_t
    {
        DATA_7_SCL_CLR, ///< data TX sequence
        DATA_7_SCL_SET, ///< data TX sequence
        DATA_6_SCL_CLR, ///< data TX sequence
        DATA_6_SCL_SET, ///< data TX sequence
        DATA_5_SCL_CLR, ///< data TX sequence
        DATA_5_SCL_SET, ///< data TX sequence
        DATA_4_SCL_CLR, ///< data TX sequence
        DATA_4_SCL_SET, ///< data TX sequence
        DATA_3_SCL_CLR, ///< data TX sequence
        DATA_3_SCL_SET, ///< data TX sequence
        DATA_2_SCL_CLR, ///< data TX sequence
        DATA_2_SCL_SET, ///< data TX sequence
        DATA_1_SCL_CLR, ///< data TX sequence
        DATA_1_SCL_SET, ///< data TX sequence
        DATA_0_SCL_CLR, ///< data TX sequence
        DATA_0_SCL_SET, ///< data TX sequence
        ACK_SDA_SET_SCL_CLR, ///< data TX sequence
        ACK_SCL_SET, ///< data TX sequence
        ACK_SCL_CLR, ///< data TX sequence
        FIRST = DATA_7_SCL_CLR, ///< first data TX sequence state
        LAST = ACK_SCL_CLR, ///< last data TX sequence state
    };

    /// Low level I2C data RX states
    enum class StateRx : uint8_t
    {
        DATA_7_SCL_SET, ///< data RX sequence
        DATA_7_SCL_CLR, ///< data RX sequence
        DATA_6_SCL_SET, ///< data RX sequence
        DATA_6_SCL_CLR, ///< data RX sequence
        DATA_5_SCL_SET, ///< data RX sequence
        DATA_5_SCL_CLR, ///< data RX sequence
        DATA_4_SCL_SET, ///< data RX sequence
        DATA_4_SCL_CLR, ///< data RX sequence
        DATA_3_SCL_SET, ///< data RX sequence
        DATA_3_SCL_CLR, ///< data RX sequence
        DATA_2_SCL_SET, ///< data RX sequence
        DATA_2_SCL_CLR, ///< data RX sequence
        DATA_1_SCL_SET, ///< data RX sequence
        DATA_1_SCL_CLR, ///< data RX sequence
        DATA_0_SCL_SET, ///< data RX sequence
        DATA_0_SCL_CLR, ///< data RX sequence
        ACK_SDA_SCL_SET, ///< data RX sequence
        ACK_SCL_CLR, ///< data RX sequence
        FIRST = DATA_7_SCL_SET, ///< first data RX sequence state
        LAST = ACK_SCL_CLR, ///< last data RX sequence state
    };

    /// Allow pre-increment definition
    friend StateStart &operator++(StateStart &);

    /// Allow pre-increment definition
    friend StateStop &operator++(StateStop &);

    /// Allow pre-increment definition
    friend StateTx &operator++(StateTx &);

    /// Allow pre-increment definition
    friend StateRx &operator++(StateRx &);

    /// Execute state machine for sending start condition.
    /// @return true if the sub-state machine is finished, after doing the work
    ///         required by the current tick.
    inline bool state_start();

    /// Execute state machine for sending the stop condition.
    /// @return true if the sub-state machine is finished, after doing the work
    ///         required by the current tick.
    inline bool state_stop();

    /// Execute data TX state machine.
    /// @param data value to send
    /// @return true if the sub-state machine is finished, after doing the work
    ///         required by the current tick. count_ may be negative to
    ///         indicate an error.
    inline bool state_tx(uint8_t data);

    /// Execute data RX state machine.
    /// @param location to shift data into
    /// @param nack send a NACK in the (N)ACK slot
    /// @return true if the sub-state machine is finished, after doing the work
    ///         required by the current tick.
    inline bool state_rx(uint8_t *data, bool nack);

    void enable() override {} /**< function to enable device */
    void disable() override {} /**< function to disable device */

    /// Method to transmit/receive the data.
    /// @param msg message to transact.
    /// @param stop produce a stop condition at the end of the transfer
    /// @return bytes transfered upon success or -1 with errno set
    inline int transfer(struct i2c_msg *msg, bool stop) override;

    /// Set the GPIO state.
    /// @param gpio GPIO to set
    void gpio_set(const Gpio *gpio)
    {
        gpio->set_direction(Gpio::Direction::DINPUT);
    }

    /// Clear the GPIO state.
    /// @param gpio GPIO to clear
    void gpio_clr(const Gpio *gpio)
    {
        gpio->clr();
        gpio->set_direction(Gpio::Direction::DOUTPUT);
        gpio->clr();
    }

    const Gpio *sda_; ///< GPIO for the SDA line
    const Gpio *scl_; ///< GPIO for the SCL line
    void (*enableTick_)(void); ///< Enable the timer tick
    void (*disableTick_)(void); ///< Disable the timer tick
    struct i2c_msg *msg_; ///< I2C message to presently act upon  
    OSSem sem_; ///< semaphore to wakeup task level from ISR
    int count_; ///< the count of data bytes transferred, error if < 0
    State state_; ///< state machine state
    union
    {
        StateStart stateStart_; ///< I2C start state machine state
        StateStop stateStop_; ///< I2C stop state machine state
        StateTx stateTx_; ///< I2C data TX state machine state
        StateRx stateRx_; ///< I2C data RX state machine state
    };
    bool stop_; ///< if true, issue stop condition at the end of the message
    bool clockStretchActive_; ///< true if the slave is clock stretching.
};

/// Pre-increment operator overload
/// @param x starting value
/// @return incremented value
inline BitBangI2C::StateStart &operator++(BitBangI2C::StateStart &x)
{
    if (x >= BitBangI2C::StateStart::FIRST && x < BitBangI2C::StateStart::LAST)
    {
        x = static_cast<BitBangI2C::StateStart>(static_cast<int>(x) + 1);
    }
    return x;
}

/// Pre-increment operator overload
/// @param x starting value
/// @return incremented value
inline BitBangI2C::StateStop &operator++(BitBangI2C::StateStop &x)
{
    if (x >= BitBangI2C::StateStop::FIRST && x < BitBangI2C::StateStop::LAST)
    {
        x = static_cast<BitBangI2C::StateStop>(static_cast<int>(x) + 1);
    }
    return x;
}

/// Pre-increment operator overload
/// @param x starting value
/// @return incremented value
inline BitBangI2C::StateTx &operator++(BitBangI2C::StateTx &x)
{
    if (x >= BitBangI2C::StateTx::FIRST && x < BitBangI2C::StateTx::LAST)
    {
        x = static_cast<BitBangI2C::StateTx>(static_cast<int>(x) + 1);
    }
    return x;
}

/// Pre-increment operator overload
/// @param x starting value
/// @return incremented value
inline BitBangI2C::StateRx &operator++(BitBangI2C::StateRx &x)
{
    if (x >= BitBangI2C::StateRx::FIRST && x < BitBangI2C::StateRx::LAST)
    {
        x = static_cast<BitBangI2C::StateRx>(static_cast<int>(x) + 1);
    }
    return x;
}


//
// BitBangI2C::tick_interrupt()
//
__attribute__((optimize("-O3")))
inline void BitBangI2C<HW>::tick_interrupt()
{
    bool exit = false;
    switch (state_)
    {
        // start sequence
        case State::START:
            if (state_start())
            {
                // start done, send the address
                state_ = State::ADDRESS;
                stateTx_ = StateTx::FIRST;
            }
            break;
        case State::ADDRESS:
        {
            /// @todo only supporting 7-bit I2C addresses at the moment.
            uint8_t address = msg_->addr << 1;
            if (msg_->flags & I2C_M_RD)
            {
                address |= 0x1;
            }
            if (state_tx(address))
            {
                if (count_ < 0)
                {
                    // Some error occured, likely an unexpected NACK. Send a
                    // stop in order to shutdown gracefully.
                    state_ = State::STOP;
                    stateStop_ = StateStop::FIRST;
                }
                else
                {
                    if (msg_->flags & I2C_M_RD)
                    {
                        state_ = State::DATA_RX;
                        stateRx_ = StateRx::FIRST;
                    }
                    else
                    {
                        state_ = State::DATA_TX;
                        stateTx_ = StateTx::FIRST;
                    }
                }
            }
            break;
        }
        case State::DATA_TX:
            if (state_tx(msg_->buf[count_]))
            {
                if (count_ < 0)
                {
                    // Some error occured, likely an unexpected NACK. Send a
                    // stop in order to shutdown gracefully.
                    state_ = State::STOP;
                    stateStop_ = StateStop::FIRST;
                }
                else if (++count_ >= msg_->len)
                {
                    // All of the data has been transmitted.
                    if (stop_)
                    {
                        state_ = State::STOP;
                        stateStop_ = StateStop::FIRST;
                    }
                    else
                    {
                        exit = true;
                    }
                }
                else
                {
                    // Setup the next byte TX
                    stateTx_ = StateTx::FIRST;
                }
            }
            break;
        case State::DATA_RX:
            if (state_rx(msg_->buf + count_, (count_ + 1 >= msg_->len)))
            {
                if (count_ < 0)
                {
                    // Some error occured, likely an unexpected NACK
                    exit = true;
                }
                else if (++count_ >= msg_->len)
                {
                    // All of the data has been received.
                    if (stop_)
                    {
                        state_ = State::STOP;
                        stateStop_ = StateStop::FIRST;
                    }
                    else
                    {
                        exit = true;
                    }
                }
                else
                {
                    // Setup the next byte RX
                    stateRx_ = StateRx::FIRST;
                }
            }
            break;
        case State::STOP:
            exit = state_stop();
            break;
    }

    if (exit)
    {
        disableTick_();
        int woken = 0;
        sem_.post_from_isr(&woken);
        os_isr_exit_yield_test(woken);
    }
}

//
// BitBangI2C::state_start()
//
__attribute__((optimize("-O3")))
inline bool BitBangI2C::state_start()
{
    switch (stateStart_)
    {
        // start sequence
        case StateStart::SDA_SET:
            gpio_set(sda_);
            ++stateStart_;
            break;
        case StateStart::SCL_SET:
            gpio_set(scl_);
            ++stateStart_;
            break;
        case StateStart::SDA_CLR:
            gpio_clr(sda_);
            ++stateStart_;
            return true;
    }
    return false;
}

//
// BitBangI2C::state_stop()
//
__attribute__((optimize("-O3")))
inline bool BitBangI2C::state_stop()
{
    switch (stateStop_)
    {
        case StateStop::SDA_CLR:
            gpio_clr(sda_);
            ++stateStop_;
            break;
        case StateStop::SCL_SET:
            gpio_set(scl_);
            ++stateStop_;
            break;
        case StateStop::SDA_SET:
            gpio_set(sda_);
            stop_ = false;
            return true; // exit
    }
    return false;
}

//
// BitBangI2C::state_tx()
//
__attribute__((optimize("-O3")))
inline bool BitBangI2C::state_tx(uint8_t data)
{
    // I2C is specified such that the data on the SDA line must be stable
    // during the high period of the clock, and the data line can only change
    // when SCL is Low.
    //
    // This means that the sequence always has to be
    // 1a. SCL := low
    // 1b. set/clear SDA
    // 2. wait a cycle for lines to settle
    // 3a. SCL := high
    // 3b. this edge is when the receiver samples
    // 4. wait a cycle, lines are stable
    switch(stateTx_)
    {
        case StateTx::DATA_7_SCL_CLR:
        case StateTx::DATA_6_SCL_CLR:
        case StateTx::DATA_5_SCL_CLR:
        case StateTx::DATA_4_SCL_CLR:
        case StateTx::DATA_3_SCL_CLR:
        case StateTx::DATA_2_SCL_CLR:
        case StateTx::DATA_1_SCL_CLR:
        case StateTx::DATA_0_SCL_CLR:
        {
            // We can only change the data when SCL is low.
            gpio_clr(scl_);
            // The divide by 2 factor (shift right by 1) is because the enum
            // states alternate between *_SCL_SET and *_SCL_CLR states in
            // increasing value.
            uint8_t mask = 0x80 >>
                ((static_cast<int>(stateTx_) -
                  static_cast<int>(StateTx::DATA_7_SCL_CLR)) >> 1);
            if (data & mask)
            {
                gpio_set(sda_);
            }
            else
            {
                gpio_clr(sda_);
            }
            ++stateTx_;
            break;
        }
        case StateTx::DATA_7_SCL_SET:
        case StateTx::DATA_6_SCL_SET:
        case StateTx::DATA_5_SCL_SET:
        case StateTx::DATA_4_SCL_SET:
        case StateTx::DATA_3_SCL_SET:
        case StateTx::DATA_2_SCL_SET:
        case StateTx::DATA_1_SCL_SET:
        case StateTx::DATA_0_SCL_SET:
            // Data is sampled by the slave following this state transition.
            gpio_set(scl_);
            ++stateTx_;
            break;
        case StateTx::ACK_SDA_SET_SCL_CLR:
            gpio_clr(scl_);
            gpio_set(sda_);
            ++stateTx_;
            break;
        case StateTx::ACK_SCL_SET:
            gpio_set(scl_);
            ++stateTx_;
            break;
        case StateTx::ACK_SCL_CLR:
            if (scl_->read() == Gpio::Value::CLR)
            {
                // Clock stretching, do the same state again.
                clockStretchActive_ = true;
                break;
            }
            if (clockStretchActive_)
            {
                // I2C spec requires minimum 4 microseconds after the SCL line
                // goes high following a clock stretch for the SCL line to be
                // pulled low again by the master. Do the same state one more
                // time to ensure this.
                clockStretchActive_ = false;
                break;
            }
            bool ack = (sda_->read() == Gpio::Value::CLR);
            gpio_clr(scl_);
            if (!ack)
            {
                count_ = -EIO;
            }
            return true; // done
    }
    return false;
}

//
// BitBangI2C::state_rx()
//
__attribute__((optimize("-O3")))
inline bool BitBangI2C::state_rx(uint8_t *data, bool nack)
{
    switch(stateRx_)
    {
        case StateRx::DATA_7_SCL_SET:
            gpio_set(sda_); // Always start with SDA high.
            // fall through
        case StateRx::DATA_6_SCL_SET:
        case StateRx::DATA_5_SCL_SET:
        case StateRx::DATA_4_SCL_SET:
        case StateRx::DATA_3_SCL_SET:
        case StateRx::DATA_2_SCL_SET:
        case StateRx::DATA_1_SCL_SET:
        case StateRx::DATA_0_SCL_SET:
            gpio_set(scl_);
            ++stateRx_;
            break;
        case StateRx::DATA_7_SCL_CLR:
        case StateRx::DATA_6_SCL_CLR:
        case StateRx::DATA_5_SCL_CLR:
        case StateRx::DATA_4_SCL_CLR:
        case StateRx::DATA_3_SCL_CLR:
        case StateRx::DATA_2_SCL_CLR:
        case StateRx::DATA_1_SCL_CLR:
        case StateRx::DATA_0_SCL_CLR:
            if (scl_->read() == Gpio::Value::CLR)
            {
                // Clock stretching, do the same state again.
                clockStretchActive_ = true;
                break;
            }
            if (clockStretchActive_)
            {
                // I2C spec requires minimum 4 microseconds after the SCL line
                // goes high following a clock stretch for the SCL line to be
                // pulled low again by the master or for the master to sample
                // the SDA data. Do the same state one more time to ensure this.
                clockStretchActive_ = false;
                break;
            }
            *data <<= 1;
            if (sda_->read() == Gpio::Value::SET)
            {
                *data |= 0x01;
            }
            gpio_clr(scl_);
            if (stateRx_ == StateRx::DATA_0_SCL_CLR && !nack)
            {
                // Send the ACK. If a NACK, SDA is already set.
                gpio_clr(sda_);
            }
            ++stateRx_;
            break;
        case StateRx::ACK_SDA_SCL_SET:
            gpio_set(scl_);
            ++stateRx_;
            break;
        case StateRx::ACK_SCL_CLR:
            if (scl_->read() == Gpio::Value::CLR)
            {
                // Clock stretching, do the same state again.
                clockStretchActive_ = true;
                break;
            }
            if (clockStretchActive_)
            {
                // I2C spec requires minimum 4 microseconds after the SCL line
                // goes high following a clock stretch for the SCL line to be
                // pulled low again by the master. Do the same state one more
                // time to ensure this.
                clockStretchActive_ = false;
                break;
            }
            gpio_clr(scl_);
            gpio_set(sda_);
            return true;
    }
    return false;
}

//
// BitBangI2C::transfer()
//
inline int BitBangI2C::transfer(struct i2c_msg *msg, bool stop)
{
    while (stop_)
    {
        // waiting for the initial "stop" condition on reset
        sem_.wait();
    }
    if (msg_->len == 0)
    {
        // Message must have length of at least 1.
        return -EINVAL;
    }
    msg_ = msg;
    count_ = 0;
    stop_ = stop;
    state_ = State::START;
    stateStart_ = StateStart::FIRST;
    enableTick_();
    sem_.wait();
    return count_;
}

#endif // _FREERTOS_DRIVERS_COMMON_BITBANG_I2C_HXX_
