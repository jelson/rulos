class Converter:
    def __init__(self, dependent_source, intermediate_file, action, script_input):
        self.dependent_source = dependent_source
        self.intermediate_file = intermediate_file
        self.action = action
        self.script_input = script_input

CONVERTERS = [
    Converter(
        dependent_source = "lib/periph/7seg_panel/display_controller.c",
        intermediate_file = "lib/periph/7seg_panel/sevseg_bitmaps.ch",
        action = "$PROJECT_ROOT/util/sevseg_convert.py $SOURCE > $TARGET",
        script_input = "lib/periph/7seg_panel/sevseg_artwork.txt"),
]
