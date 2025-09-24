[Click here](../README.md) to view the README.

## Design and implementation

The design of this application is minimalistic to get started with code examples on PSOC&trade; Edge MCU devices. All PSOC&trade; Edge E84 MCU applications have a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE). A project folder consists of various subfolders, each denoting a specific aspect of the project. The three project folders are as follows:

**Table 1. Application projects**

Project | Description
--------|------------------------
*proj_cm33_s* | Project for CM33 secure processing environment (SPE)
*proj_cm33_ns* | Project for CM33 non-secure processing environment (NSPE)
*proj_cm55* | CM55 project

In this code example, at device reset, the secure boot process starts from the ROM boot with the secure enclave (SE) as the root of trust (RoT). From the secure enclave, the boot flow is passed on to the system CPU subsystem where the secure CM33 application starts. After all necessary secure configurations, the flow is passed on to the non-secure CM33 application. Resource initialization for this example is performed by this CM33 non-secure project. It configures the system clocks, pins, clock to peripheral connections, and other platform resources. It then enables the CM55 core using the `Cy_SysEnableCM55()` function and the CM55 core is subsequently put to DeepSleep mode.

In the CM33 non-secure application, the clocks and system resources are initialized by the BSP initialization function. The retarget-io middleware is configured to use the debug UART.  

This code example uses the [lwIP](https://savannah.nongnu.org/projects/lwip) network stack, which runs multiple network timers for various network-related activities. These timers need to be serviced by the host MCU. 

Low power assistant (LPA) provides an easy way to develop low-power applications configuring PSOC&trade; Edge MCU host and WLAN (Wi-Fi/Bluetooth&reg; radio) devices to provide low-power features. LPA supports the following features:

- MCU low-power

- Wi-Fi and Bluetooth&reg; low-power

- Wi-Fi ARP offload

- Wi-Fi packet filter offload

- Wi-Fi WLAN offloads

- DHCP (Dynamic Host Configuration Protocol) Lease time offload

- ICMP (Internet Control Message Protocol) offload

- Neighbor discovery offload

- NULL Keepalive offload

- NAT (Network Address Translation) Keepalive offload

- Wake On Wireless LAN (WOWL)

- MQTT (Message Queuing Telemetry Transport) Keepalive Offload

This code example focuses on Wi-Fi WLAN offloads. See application note [AN241681](https://www.infineon.com/AN241681) (Low-power system design with PSOC&trade; Edge E84 MCU and AIROC&trade; Wi-Fi & Bluetooth&reg; combo chip) to know about these offloads in detail. 

###  Configure WLAN offloads using Device Configurator

Follow these steps to configure the offloads in the *design.modus* file using the Device Configurator tool:

1. Open Device Configurator from the **Quick Panel** when using Eclipse IDE for ModusToolbox&trade;, or through the `make config` command from the root directory of the code example repository
   >**Note:** Enable only one offload at a time in the device configurator and verify the functionality

2. On the PSOC&trade; Edge MCU **Pins** tab of the Device Configurator tool:

   1. Enable the host WAKE pin **P11[4]** and name it 'CYBSP_WIFI_HOST_WAKE'

   2. In the **Parameters** pane, set:

      - **Drive Mode:** Open Drain, Drives Low. Input buffer off

      - **Initial Drive State:** High (1)

      - **Interrupt Trigger Type:** Rising Edge
      
      **Figure 1. Setting the pin parameters in Device Configurator**

      ![](images/configurator_pins_tab.png)

     > **Note:** The Wi-Fi host driver takes care of the drive mode configuration of the host WAKE pin.

3. Go to the CYW55513IUBG tab for the connectivity device as shown in **Figure 2** and configure the following fields from Step 4. This configuration applies to all supported kits in [README](../README.md)

      **Figure 2. Packet filter configuration**

      ![](images/connectivity_tab.png)

4. **Packet filter offload:**
    
    The following packet filters are enabled. This means only these WLAN packet types will be allowed to reach the network stack of the host MCU. These are the minimum required packet types which should be allowed so the host can establish and maintain a Wi-Fi connection with the AP.

    - ARP (0x806)
    - 802.1X (0x888E)
    - DHCP (68)
    - DNS (53)

    Additionally, it allows the following packet types as the application establishes a TCP socket connection with a remote TCP server. The TCP socket connection will fail if the following packets are not allowed. Modify the port numbers to match your TCP client and server network configuration accordingly.

    - TCP client port number (50007) as both Source and Destination ports
    - TCP server port number (50007) as both Source and Destination ports

      **Figure 3. Packet filter configuration**

      ![](images/pf_configuration.png)

      **Figure 4. Packet filter configuration**

      ![](images/pf_configuration1.png)

5. **Null keepalive offload:**
    
    Enable Null Keepalive offload manually in the Device configurator as showin in **Figure 5**

    The interval parameter technically refers to the time period between transmissions of null packets, defining the frequency at which these packets are sent over a network or communication channel.

      **Figure 5. Null Keepalive configuration**

      ![](images/null_ka_configurations.png)

6. **NAT Keepalive offload:**

   Enable NAT Keepalive offload manually in the Device configurator as shown in **Figure 6**.
   
   The NAT keepalive configuration involves setting the Interval parameter to a specified number of seconds, designating a Source UDP port number, specifying a Destination UDP port number, entering a Destination IP Address, and defining the Keep Alive data payload.

      **Figure 6. NAT Keepalive configuration**

      ![](images/nat_ka_configurations.png)

7. **WakeOn Wireless LAN:**

   Enable WOWL offload manually in the Device configurator as shown in **Figure 7**

   Magic pattern should be of the below format,

   ```
   $ f"0x{dut_mac_pattern}{peer_mac_pattern}"
   ```

     >**Note:** dut_mac_pattern is the mac address of the device under test that is the first 12 bytes of the pattern and peer_mac_pattern is the mac address device that is the next 12 bytes of the pattern from which the wake pattern will be sent. It needs to be given without any space as shown **Figure 7**

      **Figure 7. WOWL configuration**

      ![](images/wowl_configurations.png)


8. **MQTT Keepalive Offload:**

   MQTT keepalive offload helps in moving this functionality to WLAN firmware so that host MCU does not need to wake up periodically to send MQTT keepalive packets and wait for the response from server.

   - Create [PSOC™ Edge MCU: Wi-Fi MQTT client](https://github.com/Infineon/mtb-example-psoc-edge-wifi-mqtt-client) code exampe from the project creator.
   - Add the LPA library from the library manager as shown in **Figure 8**
 
      **Figure 8. LPA configuration**

      ![](images/lpa_configurations.png)

   - Open and Modify the 'proj_cm33_ns/mqtt_task.c' file

   - Add necessary header files,

      ```
      /* lwIP header files */
      #include "cy_network_mw_core.h"
      #include "lwip/netif.h"

      /* Low Power Assistant header files. */
      #include "network_activity_handler.h"
      ```

   - Declare the wifi Variable inside the mqtt_client_task function. This variable will be used to store the pointer to the lwIP network interface.

      ```
      struct netif *wifi;
      ```

   - Inside mqtt_client_task function, add the code manage network activity inside the below if loop,

        if ( (CY_RSLT_SUCCESS == mqtt_init()) && (CY_RSLT_SUCCESS == mqtt_connect()) )
        {

          **** esisting code ****
          
          After this add, 
          
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

    - Enable MQTT Offload manually in the Device configurator as shown in **Figure 9**

    The MQTT wake pattern should be as shown below, 

   >**Note:** In the below pattern, {topic name} refers to the MQTT_Broker and {wake word} refers to the word wake that needs to be given along with MQTT_Broker without any space.

      ```
      {topic name}{wake word};
      ```

      **Figure 9. MQTT offload configuration**

      ![](images/mqtt_offload_configurations.png)


   The MQTT wake pattern refers to a specific sequence of data that, when received from an MQTT broker subscriber, triggers the device to wake up from its sleep state.

9. Select **File** > **Save**

   You can find the generated source files for configured offloads  in *cycfg_connectivity_wifi.c* and *cycfg_connectivity_wifi.h* in the *GeneratedSource* folder, which is located at the same location as the *design.modus* file

