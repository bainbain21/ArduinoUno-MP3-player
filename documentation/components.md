# Documentation des composants

### Microcontroller
component : Arduino UNO 

### Mp3 module
component : YX6300
pin connection :  
| pin module| pin UNO   |
| --------- | --------- |
| TX        | pin 3     |
| RX        | pin 2     |
| VCC       | 5V        |
| GND       | GND       |

### TFT screen
component : ST7789 
pin connection :  
| pin ecran | pin UNO   |
| --------- | --------- |
| VCC       | 5V        |
| GND       | GND       |
| CS        | pin 10    |
| RESET     | pin 8     |
| DC        | pin 9     |
| SDI (MOSI)| pin 11    |
| SCK       | pin 13    |
| LED       | 3,3V      |
| SCO (MISO)| pin  12   |

### Bluetooth module
component : HC-05 
pin connection :  
| pin module| pin UNO   |
| --------- | --------- |
| STATE     |           |
| RXD       | pin 4+GND | (par un pont diviseur de tension cf schematic.jpg)
| TXD       | pin 5     |
| GND       | GND       |
| VCC       | 5V        |
| KEY       | pin HIGH  |

### Encodeur rotatif
component : EC-11
pin connection :  
| pin EC    | pin UNO   |
| --------- | --------- |
| GND       | GND       |
| S1        | A2        |
| S2        | A1        |
| KEY       | A0        |
| 5V        | 5V        |

### potentiomètre coulissant
component : BK10
pin connection :  
| pin pot   | pin UNO   |
| --------- | --------- |
| broche 1  | 5V        |
| broche 2  | A3        |
| broche 3  | GND       |


### Boutons
