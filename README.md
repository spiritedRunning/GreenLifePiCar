GreenLifePiCar
==============

The project using Android smartphone to control Raspberry pi, including basic moving by motor controlling, and support gravity sensing by G-Sensor built-in mobile phone.  When Pi moving towards to the obstacle, ultrasonic detection can make Pi stop itself if distance less than 10cm e.g. It also can detect current humiture if you want. However the mobile and Pi connect with wifi. The app can also display real-time imaging througn camera install in Pi. This can use for remote video monitor.


<img src="https://github.com/spiritedRunning/GreenLifePiCar/raw/master/screenshots/20160726214000.jpg" width="30%" height="30%">
<img src="https://github.com/spiritedRunning/GreenLifePiCar/raw/master/screenshots/20160726214832.jpg" width="30%" height="30%">
<img src="https://github.com/spiritedRunning/GreenLifePiCar/raw/master/screenshots/20160726214545.jpg" width="50%" height="50%">
<img src="https://github.com/spiritedRunning/GreenLifePiCar/raw/master/screenshots/20160726214723.jpg" width="50%" height="50%">



Requirement Devices:

1. Raspberry pi Mode B Rev2
2. Meizu mx mobile phone(4.1.1)
3. DHT11 humiture sensor(3.5V-5.5V DC) / DS18B20 temperature sensor
4. DC deceleration motor * 4(12V DC)
5. L298N double H motor driver board(+5V~+35V)
6. HC-SR04 ultrasonic detect distance sensor(accuracy: 0.3cm)
7. portable source 
8. a USB camera
8. 1-way relay mode(5V, optional)