# Beats Per Minute (High-level app)

This application demonstrates how I connected a Heart Rate Sensor to the Avnet MT3620 high-level core  and display Beats Per Minute on an attached OLED Display.
The application samples and displays the output from The heart Rate sensor once per second. It uses the MT3620 analog-to-digital converter (ADC) to sample the signal.

The sample uses the following Azure Sphere libraries and requires [beta APIs](https://docs.microsoft.com/azure-sphere/app-development/use-beta).

| Library | Purpose |
|---------|---------|
| [adc](https://docs.microsoft.com/azure-sphere/reference/applibs-reference/applibs-adc/adc-overview) | Manages ADCs |
| [log](https://docs.microsoft.com/azure-sphere/reference/applibs-reference/applibs-log/log-overview) | Displays messages in the Visual Studio Device Output window during debugging |
| [eventloop](https://docs.microsoft.com/en-gb/azure-sphere/reference/applibs-reference/applibs-eventloop/eventloop-overview) | Invoke handlers for timer events |

## Contents

| File/folder | Description |
|-------------|-------------|
| ADC_HighLevelApp       | Source code and project files |
| README.md | This readme file |
| Hardware | Azure Sphere Hardware Definition|

## Prerequisites

The sample requires the following hardware:

• [Avnet Azure Sphere MT3620 Starter Kit](https://www.element14.com/community/view-product.jspa?fsku=&nsku=02AH9206&COM=noscript)

• [Heart Sensor](https://pulsesensor.com/products/pulse-sensor-amped)

• [SSD1306](https://www.amazon.com/gp/product/B072Q2X2LL/)
	128x64 Yellow Blue SSD1306 I2C OLED Display
	eg. this one for $6.99 from Amazon...
    

**Note:** By default, this sample targets[Avnet Azure Sphere MT3620 Starter Kit](https://www.element14.com/community/view-product.jspa?fsku=&nsku=02AH9206&COM=noscript) hardware. To build the sample for different Azure Sphere hardware, change the Target Hardware Definition Directory in the project properties. For detailed instructions, see the [README file in the Hardware folder](../../../Hardware/README.md).

## Prepare the sample

1. Ensure that your Azure Sphere device is connected to your computer, and your computer is connected to the internet.
1. Even if you've performed this setup previously, ensure that you have Azure Sphere SDK version 20.01 or above. At the command prompt, run **azsphere show-version** to check. Install the Azure Sphere SDK for [Windows](https://docs.microsoft.com/azure-sphere/install/install-sdk) or [Linux](https://docs.microsoft.com/azure-sphere/install/install-sdk-linux) as needed.
1. Enable application development, if you have not already done so, by entering the following line at the command prompt:

   `azsphere device enable-development`

1. Clone the [Beats Per Minute](https://github.com/skruglewicz/BeatsPerMinute) repo and find the ADC_HighLevelApp sample in the ADC folder.


## Set up the ADC connections

	Attach 3 leads tfrom the Heart Rate Sensor o following 3  pins on the Click 1 header.
	AN -- Purple
	+5v -- 5V - RED
	GND -- GND BLACK

## Build and run the sample

See the following Azure Sphere Quickstarts to learn how to build and deploy this sample:

   -  [with Visual Studio](https://docs.microsoft.com/azure-sphere/install/qs-blink-application)
   -  [with VS Code](https://docs.microsoft.com/azure-sphere/install/qs-blink-vscode)
   -  [on the Windows command line](https://docs.microsoft.com/azure-sphere/install/qs-blink-cli)
   -  [on the Linux command line](https://docs.microsoft.com/azure-sphere/install/qs-blink-linux-cli)

## Observe the output

Remote debugging from host 192.168.35.1, port 65262
ADC application starting.
OLED found!
2	77	0	600	-1000
4	76	0	600	-1000
6	76	0	600	-1000
8	75	0	600	-1000
10	78	0	600	-1000
12	76	0	600	-1000
