#include "i2c.h"  // Include I2C driver header for I2C functions

#include <stdlib.h>

static const char *TAG = "i2c";  // Define a tag for logging

#define DEV_I2C_MAX_DEVICE_COUNT 8

static DEV_I2C_Port handle;
static DEV_I2C_DeviceHandle *device_registry[DEV_I2C_MAX_DEVICE_COUNT];

static void DEV_I2C_Register_Device(DEV_I2C_DeviceHandle *dev_handle)
{
    uint8_t index;

    if (dev_handle == NULL) {
        return;
    }

    for (index = 0; index < DEV_I2C_MAX_DEVICE_COUNT; index++) {
        if (device_registry[index] == dev_handle) {
            return;
        }
    }

    for (index = 0; index < DEV_I2C_MAX_DEVICE_COUNT; index++) {
        if (device_registry[index] == NULL) {
            device_registry[index] = dev_handle;
            return;
        }
    }

    ESP_LOGW(TAG, "I2C device registry is full");
}

static void DEV_I2C_Unregister_Device(DEV_I2C_DeviceHandle *dev_handle)
{
    uint8_t index;

    for (index = 0; index < DEV_I2C_MAX_DEVICE_COUNT; index++) {
        if (device_registry[index] == dev_handle) {
            device_registry[index] = NULL;
            return;
        }
    }
}

static esp_err_t DEV_I2C_Create_Bus(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = EXAMPLE_I2C_MASTER_NUM,
        .scl_io_num = EXAMPLE_I2C_MASTER_SCL,
        .sda_io_num = EXAMPLE_I2C_MASTER_SDA,
        .glitch_ignore_cnt = 7,
    };
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &handle.bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus creation failed");
        handle.bus = NULL;
        return ret;
    }

    return ESP_OK;
}
/**
 * @brief Initialize the I2C master interface.
 * 
 * This function configures the I2C master bus and adds a device with the specified address.
 * The I2C clock source, frequency, SCL/SDA pins, and other settings are configured here.
 * 
 * @param Addr The I2C address of the device to be initialized.
 * @return The device handle if initialization is successful, NULL otherwise.
 */
DEV_I2C_Port DEV_I2C_Init()
{
    if (handle.bus == NULL) {
        memset(&handle, 0, sizeof(handle));
        memset(device_registry, 0, sizeof(device_registry));
        ESP_ERROR_CHECK(DEV_I2C_Create_Bus());
    }

    return handle;  // Return the device handle if successful
}

esp_err_t DEV_I2C_Reset_Bus()
{
    esp_err_t ret;
    uint8_t index;

    for (index = 0; index < DEV_I2C_MAX_DEVICE_COUNT; index++) {
        if (device_registry[index] != NULL && *(device_registry[index]) != NULL) {
            ret = i2c_master_bus_rm_device(*(device_registry[index]));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2C device remove failed before bus reset");
                return ret;
            }
            *(device_registry[index]) = NULL;
            device_registry[index] = NULL;
        }
    }

    if (handle.bus != NULL) {
        ret = i2c_del_master_bus(handle.bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C bus reset failed");
            return ret;
        }
        handle.bus = NULL;
    }

    return DEV_I2C_Create_Bus();
}

esp_err_t DEV_I2C_Add_Device(DEV_I2C_DeviceHandle *dev_handle, uint8_t addr)
{
    i2c_device_config_t i2c_dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = EXAMPLE_I2C_MASTER_FREQUENCY,
    };
    esp_err_t ret;

    if (dev_handle == NULL || handle.bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (*dev_handle != NULL) {
        ret = DEV_I2C_Remove_Device(dev_handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = i2c_master_bus_add_device(handle.bus, &i2c_dev_conf, dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C add device failed");
        return ret;
    }

    DEV_I2C_Register_Device(dev_handle);
    return ESP_OK;
}

esp_err_t DEV_I2C_Remove_Device(DEV_I2C_DeviceHandle *dev_handle)
{
    esp_err_t ret;

    if (dev_handle == NULL || *dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = i2c_master_bus_rm_device(*dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C remove device failed");
        return ret;
    }

    DEV_I2C_Unregister_Device(dev_handle);
    *dev_handle = NULL;
    return ESP_OK;
}

/**
 * @brief Set a new I2C slave address for the device.
 * 
 * This function allows changing the I2C slave address for the specified device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Addr The new I2C address for the device.
 */
void DEV_I2C_Set_Slave_Addr(DEV_I2C_DeviceHandle *dev_handle, uint8_t Addr)
{
    if (DEV_I2C_Add_Device(dev_handle, Addr) != ESP_OK) {
        ESP_LOGE(TAG, "I2C address modification failed");
    }
}

/**
 * @brief Retrieve the I2C bus handle.
 * 
 * This function returns the handle of the I2C bus associated with the device.
 * It is typically used when low-level operations on the bus are required or
 * when sharing the bus handle across multiple devices.
 * 
 * @return i2c_master_bus_handle_t The handle to the I2C bus.
 */
DEV_I2C_BusHandle DEV_I2C_Get_Bus_Device()
{
    return handle.bus;
}

esp_err_t DEV_I2C_Write_Reg(DEV_I2C_DeviceHandle dev_handle, uint8_t reg_addr, const uint8_t *data, size_t data_len)
{
    uint8_t *write_buffer;
    esp_err_t ret;

    if (dev_handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    write_buffer = (uint8_t *)malloc(data_len + 1U);
    if (write_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    write_buffer[0] = reg_addr;
    memcpy(&write_buffer[1], data, data_len);
    ret = i2c_master_transmit(dev_handle, write_buffer, data_len + 1U, 100);
    free(write_buffer);
    return ret;
}

esp_err_t DEV_I2C_Read_Reg(DEV_I2C_DeviceHandle dev_handle, uint8_t reg_addr, uint8_t *data, size_t data_len)
{
    if (dev_handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, data_len, 100);
}

/**
 * @brief Write a single byte to the I2C device.
 * 
 * This function sends a command byte and a value byte to the I2C device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param value The value byte to send.
 */
void DEV_I2C_Write_Byte(DEV_I2C_DeviceHandle dev_handle, uint8_t Cmd, uint8_t value)
{
    ESP_ERROR_CHECK(DEV_I2C_Write_Reg(dev_handle, Cmd, &value, 1));
}

/**
 * @brief Read a single byte from the I2C device.
 * 
 * This function reads a byte of data from the I2C device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @return The byte read from the device.
 */
uint8_t DEV_I2C_Read_Byte(DEV_I2C_DeviceHandle dev_handle)
{
    uint8_t data = 0;
    ESP_ERROR_CHECK(i2c_master_receive(dev_handle, &data, 1, 100));
    return data;
}

/**
 * @brief Read a word (2 bytes) from the I2C device.
 * 
 * This function reads two bytes (a word) from the I2C device.
 * The data is received by sending a command byte and receiving the data.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @return The word read from the device (combined two bytes).
 */
uint16_t DEV_I2C_Read_Word(DEV_I2C_DeviceHandle dev_handle, uint8_t Cmd)
{
    uint8_t data[2] = {0};
    ESP_ERROR_CHECK(DEV_I2C_Read_Reg(dev_handle, Cmd, data, 2));
    return data[1] << 8 | data[0];
}

/**
 * @brief Write multiple bytes to the I2C device.
 * 
 * This function sends a block of data to the I2C device.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param pdata Pointer to the data to send.
 * @param len The number of bytes to send.
 */
void DEV_I2C_Write_Nbyte(DEV_I2C_DeviceHandle dev_handle, uint8_t *pdata, uint8_t len)
{
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, pdata, len, 100));
}

/**
 * @brief Read multiple bytes from the I2C device.
 * 
 * This function reads multiple bytes from the I2C device.
 * The function sends a command byte and receives the specified number of bytes.
 * 
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param pdata Pointer to the buffer where received data will be stored.
 * @param len The number of bytes to read.
 */
void DEV_I2C_Read_Nbyte(DEV_I2C_DeviceHandle dev_handle, uint8_t Cmd, uint8_t *pdata, uint8_t len)
{
    ESP_ERROR_CHECK(DEV_I2C_Read_Reg(dev_handle, Cmd, pdata, len));
}
