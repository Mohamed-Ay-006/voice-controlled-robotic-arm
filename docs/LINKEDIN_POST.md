# LinkedIn post

I built an offline voice-controlled robotic arm that combines embedded AI with real-time motion control.

The ESP32-S3 captures audio from an INMP441 microphone and runs a lightweight keyword-spotting model locally—without sending voice data to the cloud. Recognized commands are transferred over UART to an ATmega32A, which controls five servo joints through a PCA9685 PWM driver.

The most valuable part of this project was not simply making the arm move. It was integrating several systems that behave very differently: digital audio, MFCC feature extraction, neural-network inference, 3.3 V/5 V communication, real-time motor control, and separate high-current power paths.

The current build is a working prototype. It can recognize selected commands and translate them into controlled physical movement, while the remaining limitations—power distribution, full command vocabulary, OLED integration, and mechanical calibration—are documented openly in the repository.

Key technologies:

- ESP32-S3 and embedded keyword spotting
- INMP441 I2S microphone
- ATmega32A firmware in C
- UART communication with logic-level conversion
- PCA9685 multi-servo PWM control
- Five-servo robotic arm

Source code, wiring, and documentation:

https://github.com/Mohamed-Ay-006/voice-controlled-robotic-arm

#EmbeddedSystems #Robotics #ESP32 #AVR #TinyML #EdgeAI #Firmware #EngineeringProjects

