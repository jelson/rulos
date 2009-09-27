#include "util.h"
#include "calculator.h"

enum {
	op_add = 0,
	op_sub = 1,
	op_mul = 2,
	op_div = 3
	};
char *operator_strs[] = {
	"add", "sub", "mul", "div" };

void calculator_notify(NotifyIfc *notify);

void calculator_init(Calculator *calc, int board0, FocusManager *fa)
{
	calc->func = calculator_notify;
	focus_init(&calc->focus);

	int i;
	for (i=0; i<2; i++)
	{
		board_buffer_init(&calc->bbuf[i]);
		board_buffer_push(&calc->bbuf[i], board0+i);
		calc->btable[i] = &calc->bbuf[i];
	}

	RectRegion calc_region = { calc->btable, 2, 1, 6 };
	focus_register(fa, (UIEventHandler*) &calc->focus, calc_region);

	RowRegion region0 = { &calc->bbuf[0], 0, 4 };
	numeric_input_init(&calc->operands[0], region0, (NotifyIfc*) calc, &calc->focus);
	RowRegion region1 = { &calc->bbuf[0], 4, 4 };
	numeric_input_init(&calc->operands[1], region1, (NotifyIfc*) calc, &calc->focus);
	RowRegion region2 = { &calc->bbuf[1], 0, 4 };
	knob_init(&calc->operator, region2, operator_strs, 4, (NotifyIfc*) calc, &calc->focus);
	RowRegion region3 = { &calc->bbuf[1], 4, 4 };
	numeric_input_init(&calc->result, region3, NULL /*unfocusable*/, NULL /* no notify */);

	DecimalFloatingPoint op0 = { 220, 0 };
	DecimalFloatingPoint op1 = { 659, 2 };
	numeric_input_set_value(&calc->operands[0], op0);
	numeric_input_set_value(&calc->operands[1], op1);
	knob_set_value(&calc->operator, op_div);
	calculator_notify((NotifyIfc*) calc);
}

char *err_overflow = "Ovrf";
char *err_divzero = "div0";
char *err_negative = "Neg ";

void calculator_notify(NotifyIfc *notify)
{
	DecimalFloatingPoint out;
	Calculator *calc = (Calculator*) notify;
	char *error = NULL;
	DecimalFloatingPoint op0 = calc->operands[0].cur_value;
	DecimalFloatingPoint op1 = calc->operands[1].cur_value;
	LOGF((logfp, "start  op0 %3de%d o1 %3de%d\n",
		op0.mantissa, op0.neg_exponent, op1.mantissa, op1.neg_exponent));
	uint32_t mantissa;
	switch (calc->operator.selected)
	{
		case op_mul:
		{
			mantissa =
				 ((uint32_t) op0.mantissa)
				*((uint32_t) op1.mantissa);
			out.neg_exponent = 
				 op0.neg_exponent
				+op1.neg_exponent;
			break;
		}
		case op_div:
		{
			mantissa = op0.mantissa;
			while (op1.neg_exponent>0)
			{
				mantissa *= 10;
				op1.neg_exponent -= 1;
			}
			if (op1.mantissa == 0)
			{
				error = err_divzero;
				goto done;
			}
			mantissa /= ((uint32_t) op1.mantissa);
			out.neg_exponent = op0.neg_exponent;

			LOGF((logfp, "div  out %3de%d\n",
				out.mantissa, out.neg_exponent));
			break;
		}
		case op_add:
		case op_sub:
		{
			// get op0 to have a larger exponent (smaller neg_exponent)
			uint8_t swapped = FALSE;
			if (op0.neg_exponent > op1.neg_exponent)
			{
				DecimalFloatingPoint tmp = op0;
				op0 = op1;
				op1 = tmp;
				swapped = TRUE;
			}
			// now multiply mantissa to align (12 => 12.0)
			uint32_t m0 = op0.mantissa;
			uint32_t m1 = op1.mantissa;
			while (op0.neg_exponent < op1.neg_exponent)
			{
				m0 *= 10;
				op0.neg_exponent += 1;
			}
			LOGF((logfp, "setup2 op0 %3de%d o1 %3de%d\n",
				m0, op0.neg_exponent, m1, op1.neg_exponent));
			assert(op0.neg_exponent == op1.neg_exponent);
			// swap mantissas back so subtraction makes sense
			if (swapped)
			{
				uint32_t tmp = m0;
				m0 = m1;
				m1 = tmp;
			}
			// now operate
			switch (calc->operator.selected)
			{
				case op_add:
					mantissa = m0 + m1;
					break;
				case op_sub:
					if (m1 > m0)
					{
						error = err_negative;
						goto done;
					}
					mantissa = m0 - m1;
					break;
			}
			out.neg_exponent = op0.neg_exponent;
		}
	}
	LOGF((logfp, "after  op0 %3de%d\n",
		mantissa, out.neg_exponent));
	// shift away digits until mantissa is legal
	while (mantissa >> 16)
	{
		if (out.neg_exponent==0)
		{
			error = err_overflow;
			goto done;
		}
		mantissa /= 10;
		out.neg_exponent -= 1;

		LOGF((logfp, "16bit  op0 %3de%d\n",
			mantissa, out.neg_exponent));
	}
	LOGF((logfp, "cleanup op0 %3de%d\n",
			mantissa, out.neg_exponent));
	out.mantissa = mantissa;
	// now shift away significant digits until it can be represented
	while (dfp_draw(&out, NULL, 4, FALSE)>4)
	{
		if (out.neg_exponent==0)
		{
			error = err_overflow;
			goto done;
		}
		out.mantissa /= 10;
		out.neg_exponent -= 1;
		LOGF((logfp, "4char  op0 %3de%d\n",
			out.mantissa, out.neg_exponent));
	}

done:
	if (error)
	{
		out.mantissa = 0;
		out.neg_exponent = 2;
		numeric_input_set_value(&calc->result, out);
		numeric_input_set_msg(&calc->result, error);
	}
	else
	{
		numeric_input_set_value(&calc->result, out);
	}
}
