# wemos_parola

A ticker written with Parola, for 2 or 3 4x8x8 led modules.
Works with esp8266

- Time via NTP
- Bitcoin and Ethereum price
- OpenWeatherMap local weather
- Stock quotes (not working because it is hard to find a reliable source)
- Pollution from french official data
- Bus schedule for a local stop from official site
- Adafruit.IO feed value with home temperature
- Custom text via web interface

2 parola zones:
- 3 8x8 GREEN modules for time
- 5 8x8 RED for scrolling contents
- All suppliers for text are idependant

- 3d Printed enclosure that can be as long as needed depending on number of modules

![Scrolling Space Invaders](images/wemos_ticker.png)
![Scrolling various information](images/wemos_ticker.mp4)
