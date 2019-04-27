// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


/*известные константы*/
#define MAX_LONG 4294967295				//максимальное значение лонга


/*конфигурация МК*/
#define POTENT 0                       // 1 - используем свое опорное напряжение, 0 - используем опрное напряжение ардуино 1.1 В
#define POT_GND A0						//земля потенциометра
#define SERIAL_SPEED 115200				//скорость обмена серийного порта	


/*настройки прошивки*/
#define DEBUG_SIGNAL 0					//1 - вывод графика входом вместо работы основной прошивки
#define DEBUG 1							//1 - вывод доп информации в сериал порт
#define SIGNAL_PIN_ENABLED 1			//использовать сигнальный пин или нет
#define USE_AUDIO_AMPLIFIEW_RELAY 1		//Использовать порт для замыкания реле усилителя
#define COUNT_OF_SIGNALS 2				//количество входных аудио-сигналов, макс - 5
#define MEANSUREMENT_AMOUNT 10000		//количество чтений порта на одно измерения
#define MEANSUREMENT_AMOUNT_FOR_NOISE 500//количество одинаковых сигналов, чтобы сигнал стал равен шуму
#define SIGNAL_ERROR 10					//разница между значениями, когда они считаются одинаковыми
#define DELAY 3000						//задержка, чтобы порт оказался (не)активным (в мс)
#define MAX_COUNT_OF_MEANSUREMENT_FOR_AMPLITUDE 1000 //масимальное количество измерений для вычисления амплитуды
#define PERIOD_ERROR 100                    //погрешность вычисления периодов
#define COUNT_OF_SAME_WAVES_FOR_NOISE 3 //количество волн с одинаковым периодом для приравнивания к шуму
#define COUNT_FOR_ZERO 5				//нижего которого значения приравнивается к нулю
#define MINIMUM_PERIOD 200				//минимальный период шума

/*пины входа/выхода*/
#define SIGNAL_PIN 13
#define AUDIO_AMPLIFIEW_RELAY 7
#define FIRST_INPUT A1
#define SECOND_INPUT A5
#define THIRD_INPUT A6
#define FOURTH_INPUT A1
#define FIFTH_INPUT A4
#define FIRST_OUTPUT 5
#define SECOND_OUTPUT 4
#define THIRD_OUTPUT 3
#define FOURTH_OUTPUT 2
#define FIFTH_OUTPUT 2

struct Signal
{
	Signal(unsigned short _input, unsigned short _output) : input(_input), output(_output)
	{
	}

	const unsigned short input;
	const unsigned short output;
	bool isMusic = false;
	unsigned long lastInactiveState;
	unsigned long lastActiveState;
};


Signal signals[] = { Signal(FIRST_INPUT, FIRST_OUTPUT), 
					 Signal(SECOND_INPUT, SECOND_OUTPUT),
					 Signal(THIRD_INPUT, THIRD_OUTPUT),
					 Signal(FOURTH_INPUT, FOURTH_OUTPUT),
					 Signal(FIFTH_INPUT, FIFTH_OUTPUT) };



void setup()
{
	
	#pragma region potentiometer
	// для увеличения точности уменьшаем опорное напряжение,
	// выставив EXTERNAL и подключив Aref к выходу 3.3V на плате через делитель
	// GND ---[10-20 кОм] --- REF --- [10 кОм] --- 3V3
	// в данной схеме GND берётся из А0 для удобства подключения
	pinMode(POT_GND, OUTPUT);
	digitalWrite(POT_GND, false);
	if (POTENT) analogReference(EXTERNAL);
	else analogReference(INTERNAL);
	#pragma endregion

	#pragma region меняем частоту оцифровки до 18 кГц
	sbi(ADCSRA, ADPS2);
	cbi(ADCSRA, ADPS1);
	sbi(ADCSRA, ADPS0);
	#pragma endregion

	#if DEBUG_SIGNAL || DEBUG || 0
	Serial.begin(SERIAL_SPEED);
	#endif

	#if SIGNAL_PIN_ENABLED
	pinMode(SIGNAL_PIN, OUTPUT);
	digitalWrite(SIGNAL_PIN, false);
	#endif

	#if USE_AUDIO_AMPLIFIEW_RELAY
	pinMode(AUDIO_AMPLIFIEW_RELAY, OUTPUT);
	#endif

	//назначение типа пинов
	for (unsigned short i = 0; i < COUNT_OF_SIGNALS; i++)
	{
		pinMode(signals[i].input, INPUT);
		pinMode(signals[i].output, OUTPUT);
		digitalWrite(signals[i].output, false);
	}
}

void loop()
{	

	#if DEBUG_SIGNAL
	Serial.print('$');
	for (unsigned short i = 0; i < COUNT_OF_SIGNALS; i++)
	{
		Serial.print(analogRead(signals[i].input));

		if (i != COUNT_OF_SIGNALS - 1)
			Serial.print(' ');
	}
	Serial.println(';');
	return;
	#endif


	#pragma region распознавание последней НЕактивности
	for (unsigned short i = 0; i < COUNT_OF_SIGNALS; i++)
	{			
		if (isMusic(signals[i].input, MEANSUREMENT_AMOUNT, MEANSUREMENT_AMOUNT_FOR_NOISE, SIGNAL_ERROR))
			signals[i].lastActiveState = millis();
		else
			signals[i].lastInactiveState = millis();
	}
	#pragma endregion

	#pragma region вычисление музыка ли это
	for (unsigned short i = 0; i < COUNT_OF_SIGNALS; i++)
	{
		unsigned long time = millis();
		unsigned long delayFromInactiveState, delayFromActiveState;

		delayFromInactiveState = time < signals[i].lastInactiveState ? (MAX_LONG - signals[i].lastInactiveState) + time : time - signals[i].lastInactiveState;
		delayFromActiveState = time < signals[i].lastActiveState ? (MAX_LONG - signals[i].lastActiveState) + time : time - signals[i].lastActiveState;

		if (delayFromInactiveState > DELAY)
			signals[i].isMusic = true;

		if (delayFromActiveState > DELAY)
			signals[i].isMusic = false;

		#if DEBUG
		Serial.print(i);
		Serial.print(": active: ");
		Serial.print(delayFromActiveState);
		Serial.print(" inactive: ");
		Serial.print(delayFromInactiveState);
		Serial.print(" state: ");
		Serial.println(signals[i].isMusic);
		#endif

	}

	#if DEBUG
	Serial.println();
	Serial.println();
	#endif
	#pragma endregion

	#pragma region назначение выходов
	bool isOpen = false;
	for (unsigned short i = 0; i < COUNT_OF_SIGNALS; i++)
	{
		if ((signals[i].isMusic) && (!isOpen))
		{
			isOpen = true;
			digitalWrite(signals[i].output, true);
		}
		else
			digitalWrite(signals[i].output, false);
	}

	#if SIGNAL_PIN_ENABLED
	digitalWrite(SIGNAL_PIN, isOpen);
	#endif

	#if USE_AUDIO_AMPLIFIEW_RELAY
	digitalWrite(AUDIO_AMPLIFIEW_RELAY, isOpen);
	#endif

	#pragma endregion

}

bool isMusic(unsigned short pin, unsigned int meansurementAmout, unsigned int meansurementAmoutForNoise, int signalError)
{
	bool result = true;
	unsigned short lastSignal = (signalError) - 1;
	int buf, amplitude = 0;
	unsigned int countOfSame = 0;
	unsigned short countOfSameWaves = 0;
	int period = 0;
	int lastPeak = 0;
	bool isZero = false;
	bool isNoise = false;
	bool itIsntNoise = false;
	int newPeriod = 0;

	Serial.println();
	Serial.println();

	for (int i = 0; i < meansurementAmout; i++)
	{
		buf = analogRead(pin);		

		if ((i > meansurementAmout / 2) || (i > MAX_COUNT_OF_MEANSUREMENT_FOR_AMPLITUDE))
		{
			if (isZero && !isNoise && !itIsntNoise)
			{
				if (buf - (signalError * 2) > amplitude)
					itIsntNoise = true;
				

				if (abs(buf - amplitude) < signalError)
				{
					newPeriod = i - lastPeak;
					if ((abs(newPeriod - period) < PERIOD_ERROR) && (newPeriod >= MINIMUM_PERIOD))
						countOfSameWaves++;
					else
						countOfSameWaves = 0;

					if (countOfSameWaves >= COUNT_OF_SAME_WAVES_FOR_NOISE)
						isNoise = true;
					


					//Serial.print("amp:");
					//Serial.print(amplitude);
					//Serial.print(";period:");
					//Serial.print(period);
					//Serial.print(";lastPeak:");
					//Serial.print(lastPeak);
					//Serial.print(";nP:");
					//Serial.print(abs(i - lastPeak - period));
					//Serial.print(";i:");
					//Serial.print(i);
					//Serial.print(";count:");
					//Serial.println(countOfSameWaves);

					period = newPeriod;
					lastPeak = i;
					isZero = false;

				}
			}
			else if (buf < COUNT_FOR_ZERO)
				isZero = true;
		}
		else
		{
			if (buf > amplitude)
				amplitude = buf;
		}


		if (abs(buf - lastSignal) < signalError)
			countOfSame++;
		else 
		{
			lastSignal = buf;
			countOfSame = 0;
			continue;
		}

		if (countOfSame >= meansurementAmoutForNoise)
		{
			result = false;
			break;
		}
	}

	#if 0
	Serial.print("pin: ");
	Serial.print(pin);
	Serial.print("; isNoise: ");
	Serial.print(isNoise);
	Serial.print("; isMusic: ");
	Serial.println(result);
	Serial.print("; itIsntNoise: ");
	Serial.println(itIsntNoise);
	Serial.print("result: ");
	Serial.println((result && !isNoise) || itIsntNoise);
	#endif

	return (result && !isNoise) || itIsntNoise;
}