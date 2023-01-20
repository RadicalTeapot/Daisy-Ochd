#pragma once
#ifndef DISPLAYDRIVER_H
#define DISPLAYDRIVER_H

#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#ifdef __cplusplus

class DisplayDriver
{
public:
    DisplayDriver() {}
    ~DisplayDriver() {}

    struct Config
    {
        Config()
        {
            // Intialize using defaults
            Defaults();
        }
        daisy::I2CHandle::Config i2c_config;
        uint8_t           i2c_address;
        uint8_t           *buffer;
        size_t            bufferSize;
        daisy::I2CHandle::CallbackFunctionPtr drawingCallback;
        void              Defaults()
        {
            i2c_config.periph         = daisy::I2CHandle::Config::Peripheral::I2C_1;
            i2c_config.speed          = daisy::I2CHandle::Config::Speed::I2C_1MHZ;
            i2c_config.mode           = daisy::I2CHandle::Config::Mode::I2C_MASTER;
            i2c_config.pin_config.scl = {DSY_GPIOB, 8};
            i2c_config.pin_config.sda = {DSY_GPIOB, 9};
            i2c_address               = 0x3C;

            drawingCallback = NULL;
        }
    };

    void Init(const Config &config)
    {
        buffer_ = config.buffer;
        bufferSize_ = config.bufferSize;

        drawingCallback_ = config.drawingCallback;

        i2c_address_ = config.i2c_address;
        i2c_.Init(config.i2c_config);

        // Init routine...

        // Display Off
        SendCommand(0xaE);

        // Dimension dependent commands...
        // Display Clock Divide Ratio
        SendCommand(0xD5);
        SendCommand(0x80);
        // Multiplex Ratio
        SendCommand(0xA8);
        SendCommand(0x3F);
        // COM Pins
        SendCommand(0xDA);
        SendCommand(0x12);

        // Display Offset
        SendCommand(0xD3);
        SendCommand(0x00);
        // Start Line Address
        SendCommand(0x40);
        // Normal Display
        SendCommand(0xA6);
        // All On Resume
        SendCommand(0xA4);
        // Charge Pump
        SendCommand(0x8D);
        SendCommand(0x14);
        // Set Segment Remap
        SendCommand(0xA1);
        // COM Output Scan Direction
        SendCommand(0xC8);
        // Contrast Control
        SendCommand(0x81);
        SendCommand(0x8F);
        // Pre Charge
        SendCommand(0xD9);
        SendCommand(0x25);
        // VCOM Detect
        SendCommand(0xDB);
        SendCommand(0x34);

        // Display On
        SendCommand(0xAF); //--turn on oled panel
    };

    void SendCommand(uint8_t cmd)
    {
        uint8_t buf[2] = {0X00, cmd};
        i2c_.TransmitBlocking(i2c_address_, buf, 2, 1000);
    };

    void SendBufferData()
    {
        memmove(buffer_, buffer_+1, bufferSize_-1);
        buffer_[0] = 0x40;
        i2c_.TransmitDma(i2c_address_, buffer_, bufferSize_, drawingCallback_, NULL);
    }

    void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on)
    {
        if (x >= width_ || y >= height_)
            return;
        if (on)
            buffer_[x + (y / 8) * width_] |= (1 << (y % 8));
        else
            buffer_[x + (y / 8) * width_] &= ~(1 << (y % 8));
    }

    void Fill(bool on)
    {
        for (size_t i = 0; i < bufferSize_; i++)
        {
            buffer_[i] = on ? 0xff : 0x00;
        }
    };

    /**
     * Update the display
     */
    void Update()
    {
        // Set to horizontal addressing mode
        SendCommand(0x20);  // Set in Memory addressing mode
        SendCommand(0x00);  // Set to horizontal
        // Set column start and end addresses
        SendCommand(0x21);  // Set in column address mode
        SendCommand(0);     // Set start column
        SendCommand(127);   // Set end column
        // Set page start and end addresses
        SendCommand(0x22);  // Set in page address mode
        SendCommand(0);     // Set start page
        SendCommand(7);     // Set end page
        SendBufferData();
    };

    uint16_t Height() const { return height_; }
    uint16_t Width() const { return width_; }

private:
    daisy::I2CHandle i2c_;
    uint8_t i2c_address_;
    static const size_t width_ = 128;
    static const size_t height_ = 64;
    uint8_t *buffer_;
    size_t bufferSize_;
    daisy::I2CHandle::CallbackFunctionPtr drawingCallback_;
};

#endif
#endif
