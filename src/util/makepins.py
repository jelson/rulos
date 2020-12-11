#!/usr/bin/env python3

# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

ATMEL_FMT = '''
#define GPIO_{port}{pin} (gpio_pin_t) {{ \
    .ddr = &DDR{port}, \
    .port = &PORT{port}, \
    .pin = &PIN{port}, \
    .bit = PORT{port}{pin}, \
}}
'''

def atmel():
    print('// automatically generated pin definitions, do not edit')
    for port in ['A', 'B', 'C', 'D']:
        for pin in range(8):
            print(ATMEL_FMT.format(port=port, pin=pin))

STM32_FMT = '''
static const gpio_pin_t GPIO_{port}{pin} = {{
   .port = GPIO{port},
   .pin = LL_GPIO_PIN_{pin},
}};
'''


def stm32():
    print('// automatically generated pin definitions, do not edit')
    for port in ['A', 'B', 'C', 'D']:
        for pin in range(16):
            print(STM32_FMT.format(port=port, pin=pin))

#stm32()
atmel()


