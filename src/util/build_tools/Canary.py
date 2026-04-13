# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
# (jelson@gmail.com).
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

from .ArmBuildRules import ArmStmPlatform
from .AvrBuildRules import AvrPlatform
from .Esp32BuildRules import Esp32Platform
from .SimBuildRules import SimulatorPlatform

# One representative per AVR family RULOS has apps for. Different chips
# within the same family (e.g. atmega328 vs atmega328p, attiny84 vs
# attiny84a) compile identically for our purposes, so only one is
# needed to catch driver breakage.
_ATMEGA_REPRESENTATIVES = ("atmega32", "atmega328", "atmega1284")
_ATTINY_REPRESENTATIVES = ("attiny84", "attiny85")


def _stm32_line_representatives(families):
    """One chip per STM32 chip line (e.g. stm32g431, stm32g474, stm32f103)
    — coarser than per-density/package but finer than per-major-family, so
    that e.g. G431 and G474 both get exercised even though both are G4.
    Picks the highest-RAM (ties broken by highest-flash) variant of each
    line so tests with nontrivial memory needs fit."""
    by_line = {}  # chip_line -> chip object with the most RAM/flash seen
    for chip_name in ArmStmPlatform.CHIPS:
        chip = ArmStmPlatform.CHIPS[chip_name]
        if families is not None and chip.major_family_name not in families:
            continue
        chip_line = chip_name[:9]  # e.g. "stm32g431" from "stm32g431x8"
        current = by_line.get(chip_line)
        if current is None or (chip.ramk, chip.flashk) > (current.ramk, current.flashk):
            by_line[chip_line] = chip
    return [chip.name for chip in sorted(by_line.values(), key=lambda c: c.name)]


def canary_platforms(
    stm32_families=None,
    include_atmega=True,
    include_attiny=True,
    include_esp32=True,
    include_sim=True,
    extras=(),
):
    """Return a list of Platform instances for canary builds: one
    representative chip per family RULOS knows about. Useful for tests
    that exercise a shared driver across every architecture so that any
    family's breakage is caught.

    stm32_families: iterable of STM32 major family names (e.g. "STM32G4")
        to include. None means every family.
    include_atmega, include_attiny, include_esp32, include_sim: toggle
        each AVR subfamily, ESP32, and the host simulator. Turn off the
        ones a particular driver doesn't support (e.g. UART doesn't
        build on ATtiny since those chips have USI, not USART).
    extras: additional Platform instances to append — typically the
        specific chip used in a hardware test jig.
    """
    platforms = []
    for chip_name in _stm32_line_representatives(stm32_families):
        platforms.append(ArmStmPlatform(chip_name))
    if include_atmega:
        for chip_name in _ATMEGA_REPRESENTATIVES:
            platforms.append(AvrPlatform(chip_name))
    if include_attiny:
        for chip_name in _ATTINY_REPRESENTATIVES:
            platforms.append(AvrPlatform(chip_name))
    if include_esp32:
        platforms.append(Esp32Platform())
    if include_sim:
        platforms.append(SimulatorPlatform())
    platforms.extend(extras)
    return platforms
