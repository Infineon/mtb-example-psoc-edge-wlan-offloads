/*******************************************************************************
* File Name:   tcp_keepalive_offload.c
*
* Description: This file contains task and functions related to TCP client
* operation.
*
* Related Document: See README.md
*
********************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cybsp.h"
#include "cyabs_rtos.h"
#include <string.h>
#include "retarget_io_init.h"

/*Secure socket header file. */
#include "cy_secure_sockets.h"

/* Wi-Fi connection manager header files. */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

/* TCP client task header file. */
#include "tcp_keepalive_offload.h"

/* IP address related header files. */
#include "cy_nw_helper.h"

/* Standard C header files */
#include <inttypes.h>
/* lwIP header files */
#include "cy_network_mw_core.h"
#include "lwip/netif.h"

/* Low Power Assistant header files. */
#include "network_activity_handler.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* To use the Wi-Fi device in AP interface mode, set this macro as '1' */

#define TCP_KEEPALIVE_OFFLOAD                    (0U)


#define MAKE_IP_PARAMETERS(a, b, c, d)           ((((uint32_t) d) << 24) | \
                                                 (((uint32_t) c) << 16) | \
                                                 (((uint32_t) b) << 8) |\
                                                 ((uint32_t) a))

#define WIFI_INTERFACE_TYPE                     CY_WCM_INTERFACE_TYPE_STA

/* Wi-Fi Credentials: Modify WIFI_SSID, WIFI_PASSWORD, and WIFI_SECURITY_TYPE
 * to match your Wi-Fi network credentials.
 * Note: Maximum length of the Wi-Fi SSID and password is set to
 * CY_WCM_MAX_SSID_LEN and CY_WCM_MAX_PASSPHRASE_LEN as defined in cy_wcm.h file.
 */
#define WIFI_SSID                                "MY_WIFI_SSID"
#define WIFI_PASSWORD                            "MY_WIFI_PASSWORD"

/* Security type of the Wi-Fi access point. See 'cy_wcm_security_t' structure
 * in "cy_wcm.h" for more details.
 */
#define WIFI_SECURITY_TYPE                        CY_WCM_SECURITY_WPA2_AES_PSK
/* Maximum number of connection retries to a Wi-Fi network. */
#define MAX_WIFI_CONN_RETRIES                     (10U)

/* Wi-Fi re-connection time interval in milliseconds */
#define WIFI_CONN_RETRY_INTERVAL_MSEC             (1000U)


/* Maximum number of connection retries to the TCP server. */
#define MAX_TCP_SERVER_CONN_RETRIES               (5U)

/* Length of the TCP data packet. */
#define MAX_TCP_DATA_PACKET_LENGTH                (20u)

/* TCP keep alive related macros. */
#define TCP_KEEP_ALIVE_IDLE_TIME_MS               (10000U)
#define TCP_KEEP_ALIVE_INTERVAL_MS                (1000U)
#define TCP_KEEP_ALIVE_RETRY_COUNT                (2U)

/* Length of the LED ON/OFF command issued from the TCP server. */
#define TCP_LED_CMD_LEN                           (1U)
#define LED_ON_CMD                                '1'
#define LED_OFF_CMD                               '0'
#define ACK_LED_ON                                "LED ON ACK"
#define ACK_LED_OFF                               "LED OFF ACK"
#define MSG_INVALID_CMD                           "Invalid command"

#define TCP_SERVER_PORT                           (50007U)
#define ASCII_BACKSPACE                           (0x08)
#define RTOS_TICK_TO_WAIT                         (1U)
#define UART_INPUT_TIMEOUT_MS                     (1U)
#define UART_BUFFER_SIZE                          (20U)

#define SEMAPHORE_LIMIT                           (1U)

#define INIT_COUNT_FOR_SEMAPHORE                  (0U)
#define CARRIAGE_RETURN                           ('\r')
#define NEWLINE                                   ('\n')
#define BACKSPACE                                 ('\b')
#define NULLCHARACTER                             ('\0')
#define DISCONNECTION_TIMEOUT                     (0U)
#define VALUE_TO_BE_FILLED                        (0U)
#define BYTE_ZERO                                 (0U)
#define RESET_VAL                                 (0U)

#define APP_SDIO_INTERRUPT_PRIORITY               (7U)
#define APP_HOST_WAKE_INTERRUPT_PRIORITY          (2U)
#define APP_SDIO_FREQUENCY_HZ                     (25000000U)
#define SDHC_SDIO_64BYTES_BLOCK                   (64U)

/* This macro specifies the interval in milliseconds that the device monitors
 * the network for inactivity. If the network is inactive for duration lesser 
 * than INACTIVE_WINDOW_MS in this interval, the MCU does not suspend the network 
 * stack and informs the calling function that the MCU wait period timed out 
 * while waiting for network to become inactive.
 */
#define INACTIVE_INTERVAL_MS                     (300U)

/* This macro specifies the continuous duration in milliseconds for which the 
 * network has to be inactive. If the network is inactive for this duaration,
 * the MCU will suspend the network stack. Now, the MCU will not need to service
 * the network timers which allows it to stay longer in sleep/deepsleep.
 */
#define INACTIVE_WINDOW_MS                       (200U)
#define INTERFACE_ID                             (0U)

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
cy_rslt_t create_tcp_client_socket(void);
cy_rslt_t tcp_disconnection_handler(cy_socket_t socket_handle, void *arg);
cy_rslt_t connect_to_tcp_server(cy_socket_sockaddr_t address);

static cy_rslt_t connect_to_wifi_ap(void);

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* TCP client socket handle */
cy_socket_t client_handle;

/* Binary semaphore handle to keep track of TCP server connection. */
cy_semaphore_t connect_to_server;

/* Holds the IP address obtained for SoftAP using Wi-Fi Connection Manager (WCM). */
cy_wcm_ip_address_t softap_ip_address;

extern cy_stc_scb_uart_context_t    DEBUG_UART_context;
static mtb_hal_sdio_t sdio_instance;
static cy_stc_sd_host_context_t sdhc_host_context;
static cy_wcm_config_t wcm_config;


#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)

/* SysPm callback parameter structure for SDHC */
static cy_stc_syspm_callback_params_t sdcardDSParams =
{
    .context   = &sdhc_host_context,
    .base      = CYBSP_WIFI_SDIO_HW
};

/* SysPm callback structure for SDHC*/
static cy_stc_syspm_callback_t sdhcDeepSleepCallbackHandler =
{
    .callback           = Cy_SD_Host_DeepSleepCallback,
    .skipMode           = SYSPM_SKIP_MODE,
    .type               = CY_SYSPM_DEEPSLEEP,
    .callbackParams     = &sdcardDSParams,
    .prevItm            = NULL,
    .nextItm            = NULL,
    .order              = SYSPM_CALLBACK_ORDER
};
#endif

/*******************************************************************************
* Function Name: sdio_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for SDIO instance.
*
*******************************************************************************/
static void sdio_interrupt_handler(void)
{
    mtb_hal_sdio_process_interrupt(&sdio_instance);
}

/*******************************************************************************
* Function Name: host_wake_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for the host wake up input pin.
*******************************************************************************/
static void host_wake_interrupt_handler(void)
{
    mtb_hal_gpio_process_interrupt(&wcm_config.wifi_host_wake_pin);
}

/*******************************************************************************
* Function Name: app_sdio_init
********************************************************************************
* Summary:
* This function configures and initializes the SDIO instance used in 
* communication between the host MCU and the wireless device.
*
*******************************************************************************/
static void app_sdio_init(void)
{
    cy_rslt_t result;
    mtb_hal_sdio_cfg_t sdio_hal_cfg;
    
    cy_stc_sysint_t sdio_intr_cfg =
    {
        .intrSrc = CYBSP_WIFI_SDIO_IRQ,
        .intrPriority = APP_SDIO_INTERRUPT_PRIORITY
    };

    cy_stc_sysint_t host_wake_intr_cfg =
    {
            .intrSrc = CYBSP_WIFI_HOST_WAKE_IRQ,
            .intrPriority = APP_HOST_WAKE_INTERRUPT_PRIORITY
    };

    /* Initialize the SDIO interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status = Cy_SysInt_Init(&sdio_intr_cfg, sdio_interrupt_handler);

    /* SDIO interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_SDIO_IRQ);

    /* Setup SDIO using the HAL object and desired configuration */
    result = mtb_hal_sdio_setup(&sdio_instance, &CYBSP_WIFI_SDIO_sdio_hal_config, NULL, &sdhc_host_context);

    /* SDIO setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Initialize and Enable SD HOST */
    Cy_SD_Host_Enable(CYBSP_WIFI_SDIO_HW);
    Cy_SD_Host_Init(CYBSP_WIFI_SDIO_HW, CYBSP_WIFI_SDIO_sdio_hal_config.host_config, &sdhc_host_context);
    Cy_SD_Host_SetHostBusWidth(CYBSP_WIFI_SDIO_HW, CY_SD_HOST_BUS_WIDTH_4_BIT);

    sdio_hal_cfg.frequencyhal_hz = APP_SDIO_FREQUENCY_HZ;
    sdio_hal_cfg.block_size = SDHC_SDIO_64BYTES_BLOCK;

    /* Configure SDIO */
    mtb_hal_sdio_configure(&sdio_instance, &sdio_hal_cfg);

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
    /* SDHC SysPm callback registration */
    Cy_SysPm_RegisterCallback(&sdhcDeepSleepCallbackHandler);
#endif /* (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP) */

    /* Setup GPIO using the HAL object for WIFI WL REG ON  */
    mtb_hal_gpio_setup(&wcm_config.wifi_wl_pin, CYBSP_WIFI_WL_REG_ON_PORT_NUM, CYBSP_WIFI_WL_REG_ON_PIN);

    /* Setup GPIO using the HAL object for WIFI HOST WAKE PIN  */
    mtb_hal_gpio_setup(&wcm_config.wifi_host_wake_pin, CYBSP_WIFI_HOST_WAKE_PORT_NUM, CYBSP_WIFI_HOST_WAKE_PIN);

    /* Initialize the Host wakeup interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status_host_wake =  Cy_SysInt_Init(&host_wake_intr_cfg, host_wake_interrupt_handler);

    /* Host wake up interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status_host_wake)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_HOST_WAKE_IRQ);
}

#if(TCP_KEEPALIVE_OFFLOAD)
/*******************************************************************************
* Function Name: uart_bytesAvailable
********************************************************************************
* Summary:
*  Checks the number of bytes available to read from the receive buffers
*
* Return:
*  uint32_t number_available: Return The number of readable bytes
*
*******************************************************************************/
static uint32_t uart_bytesAvailable(void)
{
    uint32_t numofBytes_available = Cy_SCB_UART_GetNumInRxFifo(SCB2);
    if(NULL != DEBUG_UART_context.rxRingBuf )
    {
        numofBytes_available += Cy_SCB_UART_GetNumInRingBuffer(SCB2, &DEBUG_UART_context);
    }
    return numofBytes_available;
}

/*******************************************************************************
* Function Name: uart_get_data
********************************************************************************
* Summary:
*  Reads the input data received from uart
*
* Parameters:
*  value : The value read from the serial port
*
*******************************************************************************/
static void uart_get_data(uint8_t *value)
{
    uint32_t read_value = Cy_SCB_UART_Get(SCB2);
    while (CY_SCB_UART_RX_NO_DATA == read_value)
    {
        read_value = Cy_SCB_UART_Get(SCB2);
    }
    *value = (uint8_t)read_value;
}

/*******************************************************************************
* Function Name: uart_put
********************************************************************************
* Summary:
*  Sends a character
*
* Parameters:
*  value : The character to be sent
*
*******************************************************************************/
void uart_put(uint32_t value)
{
  Cy_SCB_UART_Put(SCB2, value);

}

/*******************************************************************************
* Function Name: read_uart_input
********************************************************************************
* Summary:
*  Function to read user input from UART terminal.
*
* Parameters:
*  uint8_t* input_buffer_ptr: Pointer to input buffer
*
*******************************************************************************/
static void read_uart_input(uint8_t* input_buffer_ptr)
{
    uint8_t *input_ptr = input_buffer_ptr;
    uint32_t numBytes;

    do
    {
        /* Check for data in the UART buffer */
        numBytes = uart_bytesAvailable();

        if(numBytes)
        {
            uart_get_data(input_ptr);

            if((CARRIAGE_RETURN == *input_ptr) || (NEWLINE == *input_ptr))
            {
                printf("\n");
            }
            else
            {
                /* Echo the received character */
                uart_put(*input_ptr);
                if (BACKSPACE != *input_ptr)
                {
                    input_ptr++;
                }
                else if(input_ptr != input_buffer_ptr)
                {
                    input_ptr--;
                }
            }
        }

        cy_rtos_delay_milliseconds(RTOS_TICK_TO_WAIT);

    } while((*input_ptr != CARRIAGE_RETURN) && (*input_ptr != NEWLINE));

    /* Terminate string with NULL character. */
    *input_ptr = NULLCHARACTER;
}
#endif

/*******************************************************************************
* Function Name: network_idle_task
********************************************************************************
* Summary:
*  Task used to establish a connection to a remote TCP server and
*  control the LED state (ON/OFF) based on the command received from TCP server.
*
* Parameters:
*  void *args : Task parameter defined during task creation (unused).
*
*******************************************************************************/
void network_idle_task(void *arg)
{
    struct netif *wifi;
    cy_rslt_t result ;
    
#if(TCP_KEEPALIVE_OFFLOAD)
    uint8_t uart_input[UART_BUFFER_SIZE];

    /* IP address and TCP port number of the TCP server to which the TCP client
     * connects to.
     */
    cy_socket_sockaddr_t tcp_server_address =
    {
        .ip_address.version = CY_SOCKET_IP_VER_V4,
        .port = TCP_SERVER_PORT
    };

    /* IP variable for network utility functions */
    cy_nw_ip_address_t nw_ip_addr =
    {
        .version = NW_IP_IPV4
    };
#endif

    app_sdio_init();

    wcm_config.interface = WIFI_INTERFACE_TYPE;
    wcm_config.wifi_interface_instance = &sdio_instance;

    /* Initialize Wi-Fi connection manager. */
    result = cy_wcm_init(&wcm_config);

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Wi-Fi Connection Manager initialization failed! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
       
        handle_app_error();
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

    /* Connect to Wi-Fi AP */
    result = connect_to_wifi_ap();
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\n Failed to connect to Wi-Fi AP! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        handle_app_error();
    }
    
#if(TCP_KEEPALIVE_OFFLOAD)
    /* Create a binary semaphore to keep track of TCP server connection. */
    cy_rtos_semaphore_init(&connect_to_server, SEMAPHORE_LIMIT, INIT_COUNT_FOR_SEMAPHORE);

    /* Give the semaphore so as to connect to TCP server.  */
    cy_rtos_semaphore_set(&connect_to_server);

    /* Initialize secure socket library. */
    result = cy_socket_init();

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Secure Socket initialization failed!\n");
        handle_app_error();
    }
    printf("Secure Socket initialized\n");

    /* Wait till semaphore is acquired so as to connect to a TCP server. */
    cy_rtos_semaphore_get(&connect_to_server, CY_RTOS_NEVER_TIMEOUT);

    printf("Connect to TCP server\n");
    printf("Enter the IPv4 address of the TCP Server:\n");

    /* Clear the UART input buffer. */
    memset(uart_input, VALUE_TO_BE_FILLED, UART_BUFFER_SIZE);

    /* Read the TCP server's IPv4 address from  the user via the
     * UART terminal.
     */
    read_uart_input(uart_input);

    cy_nw_str_to_ipv4((char *)uart_input, (cy_nw_ip_address_t *)&nw_ip_addr);
    tcp_server_address.ip_address.ip.v4 = nw_ip_addr.ip.v4;

    /* Connect to the TCP server. If the connection fails, retry
     * to connect to the server for MAX_TCP_SERVER_CONN_RETRIES times.
     */
    cy_nw_ntoa(&nw_ip_addr, (char *)&uart_input);
    printf("Connecting to TCP Server (IP Address: %s, Port: %d)\n\n",
                  uart_input, TCP_SERVER_PORT);

    result = connect_to_tcp_server(tcp_server_address);

    if(CY_RSLT_SUCCESS != result)
    {
        printf("Failed to connect to TCP server.\n");

        /* Give the semaphore so as to connect to TCP server.  */
        cy_rtos_semaphore_set(&connect_to_server);
    }
#endif
        
          /* Obtain the pointer to the lwIP network interface. This pointer is used to
    * access the Wi-Fi driver interface to configure the WLAN power-save mode.
    */
    wifi = (struct netif*)cy_network_get_nw_interface
                         (CY_NETWORK_WIFI_STA_INTERFACE, INTERFACE_ID);

    while (true)
    {
       /* Configures an emac activity callback to the Wi-Fi interface and
        * suspends the network if the network is inactive for a duration of
        * INACTIVE_WINDOW_MS inside an interval of INACTIVE_INTERVAL_MS. The
        * callback is used to signal the presence/absence of network activity
        * to resume/suspend the network stack.
        */
        wait_net_suspend(wifi, portMAX_DELAY, INACTIVE_INTERVAL_MS,
                INACTIVE_WINDOW_MS);
    }

 }

/*******************************************************************************
* Function Name: connect_to_wifi_ap()
********************************************************************************
* Summary:
*  Connects to Wi-Fi AP using the user-configured credentials, retries up to a
*  configured number of times until the connection succeeds.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t:  Returns CY_RSLT_SUCCESS if the Wi-Fi AP connection is successful.
*
*******************************************************************************/
cy_rslt_t connect_to_wifi_ap(void)
{
    cy_rslt_t result;
    char ip_addr_str[UART_BUFFER_SIZE];

    /* Variables used by Wi-Fi connection manager.*/
    cy_wcm_connect_params_t wifi_conn_param;
    cy_wcm_ip_address_t ip_address;

    /* IP variable for network utility functions */
    cy_nw_ip_address_t nw_ip_addr =
    {
        .version = NW_IP_IPV4
    };

     /* Set the Wi-Fi SSID, password and security type. */
    memset(&wifi_conn_param, RESET_VAL, sizeof(cy_wcm_connect_params_t));
    memcpy(wifi_conn_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_conn_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    wifi_conn_param.ap_credentials.security = WIFI_SECURITY_TYPE;

    printf("Connecting to Wi-Fi Network: %s\n", WIFI_SSID);

    /* Join the Wi-Fi AP. */
    for(uint32_t conn_retries = 0; conn_retries < MAX_WIFI_CONN_RETRIES; conn_retries++ )
    {
        result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);

        if(CY_RSLT_SUCCESS == result)
        {
            printf("Successfully connected to Wi-Fi network '%s'.\n",
                                wifi_conn_param.ap_credentials.SSID);
            nw_ip_addr.ip.v4 = ip_address.ip.v4;
            cy_nw_ntoa(&nw_ip_addr, ip_addr_str);
            printf("IP Address Assigned: %s\n", ip_addr_str);
            return result;
        }

        printf("Connection to Wi-Fi network failed with error code %d."
               "Retrying in %d ms...\n", (int)result, WIFI_CONN_RETRY_INTERVAL_MSEC);

        cy_rtos_delay_milliseconds(WIFI_CONN_RETRY_INTERVAL_MSEC);
    }

    /* Stop retrying after maximum retry attempts. */
    printf("Exceeded maximum Wi-Fi connection attempts\n");

    return result;
}

/*******************************************************************************
* Function Name: create_tcp_client_socket
********************************************************************************
* Summary:
*  Function to create a socket and set the socket options
*  to set call back function for handling incoming messages, call back
*  function to handle disconnection.
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: Returns CY_RSLT_SUCCESS if the TCP server socket is created
* successfully.
*
*******************************************************************************/
cy_rslt_t create_tcp_client_socket(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;;

    /* TCP keep alive parameters. */
    int keep_alive = 1;
#if defined (COMPONENT_LWIP)
    uint32_t keep_alive_interval = TCP_KEEP_ALIVE_INTERVAL_MS;
    uint32_t keep_alive_count    = TCP_KEEP_ALIVE_RETRY_COUNT;
    uint32_t keep_alive_idle_time = TCP_KEEP_ALIVE_IDLE_TIME_MS;
#endif

    /* Variables used to set socket options. */
    cy_socket_opt_callback_t tcp_recv_option;
    cy_socket_opt_callback_t tcp_disconnect_option;

    /* Create a new secure TCP socket. */
    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_STREAM,
                              CY_SOCKET_IPPROTO_TCP, &client_handle);

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Failed to create socket!\n");
        return result;
    }

    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                  CY_SOCKET_SO_RECEIVE_CALLBACK,
                                  &tcp_recv_option, sizeof(cy_socket_opt_callback_t));
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_RECEIVE_CALLBACK failed\n");
        return result;
    }

    /* Register the callback function to handle disconnection. */
    tcp_disconnect_option.callback = tcp_disconnection_handler;
    tcp_disconnect_option.arg = NULL;

    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                  CY_SOCKET_SO_DISCONNECT_CALLBACK,
                                  &tcp_disconnect_option, sizeof(cy_socket_opt_callback_t));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_DISCONNECT_CALLBACK failed\n");
    }

#if defined (COMPONENT_LWIP)
    /* Set the TCP keep alive interval. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL,
                                  &keep_alive_interval, sizeof(keep_alive_interval));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL failed\n");
        return result;
    }

    /* Set the retry count for TCP keep alive packet. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_COUNT,
                                  &keep_alive_count, sizeof(keep_alive_count));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_TCP_KEEPALIVE_COUNT failed\n");
        return result;
    }

    /* Set the network idle time before sending the TCP keep alive packet. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME,
                                  &keep_alive_idle_time, sizeof(keep_alive_idle_time));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME failed\n");
        return result;
    }
#endif

    /* Enable TCP keep alive. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                      CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE,
                                          &keep_alive, sizeof(keep_alive));
    if(CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE failed\n");
        return result;
    }

    return result;
}

/*******************************************************************************
* Function Name: connect_to_tcp_server
********************************************************************************
* Summary:
*  Function to connect to TCP server.
*
* Parameters:
*  cy_socket_sockaddr_t address: Address of TCP server socket
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if a successful
*  connection to the TCP server was established.
*
*******************************************************************************/
cy_rslt_t connect_to_tcp_server(cy_socket_sockaddr_t address)
{
    cy_rslt_t result = CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;
    cy_rslt_t conn_result= CY_RSLT_SUCCESS;

    for(uint32_t conn_retries = 0; conn_retries < MAX_TCP_SERVER_CONN_RETRIES; conn_retries++)
    {
        /* Create a TCP socket */
        conn_result = create_tcp_client_socket();

        if(CY_RSLT_SUCCESS != conn_result)
        {
            printf("Socket creation failed!\n");
            handle_app_error();
        }

        conn_result = cy_socket_connect(client_handle, &address, sizeof(cy_socket_sockaddr_t));

        if (CY_RSLT_SUCCESS == conn_result)
        {
            printf("============================================================\n");
            printf("Connected to TCP server\n");

            return conn_result;
        }

        printf("Could not connect to TCP server. Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        printf("Trying to reconnect to TCP server... Please check if the server is listening\n");

        /* The resources allocated during the socket creation (cy_socket_create)
         * should be deleted.
         */
        cy_socket_delete(client_handle);
    }

     /* Stop retrying after maximum retry attempts. */
     printf("Exceeded maximum connection attempts to the TCP server\n");

     return result;
}


/*******************************************************************************
* Function Name: tcp_disconnection_handler
********************************************************************************
* Summary:
*  Callback function to handle TCP socket disconnection event.
*
* Parameters:
*  cy_socket_t socket_handle: Connection handle for the TCP client socket
*  void *args : Parameter passed on to the function (unused)
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if the socket disconnected
*
*******************************************************************************/
cy_rslt_t tcp_disconnection_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Disconnect the TCP client. */
    result = cy_socket_disconnect(socket_handle, DISCONNECTION_TIMEOUT);

    /* Free the resources allocated to the socket. */
    cy_socket_delete(socket_handle);

    printf("Disconnected from the TCP server! \n");

    /* Give the semaphore so as to connect to TCP server. */
    cy_rtos_semaphore_set(&connect_to_server);

    return result;
}

/* [] END OF FILE */
