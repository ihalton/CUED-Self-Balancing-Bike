#pragma config(Sensor, S1,     gyroSensor,     sensorEV3_Gyro, modeEV3Gyro_Rate)
#pragma config(Motor,  motorA,          driveMotLeft,  tmotorEV3_Large, openLoop, driveLeft, encoder)
#pragma config(Motor,  motorB,          driveMotRight, tmotorEV3_Large, openLoop, driveRight)
#pragma config(Motor,  motorC,          handlebarMotor, tmotorEV3_Large, openLoop, encoder)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

#include "kalmangyr.h"
#include "tf.h"

#define DEG2RAD (PI/180.0)
#define RAD2DEG (180.0/PI)

// General settings
const float wheelRadius = 0.04;
const float T = 0.01;

// Controller
const float Kph = 12.0; // 12
const float Kdh = 0.5;  // 0.1

float tau = 0.01;
float prevmeas = 0.0;
float hderiv = 0.0;
float hderivprev = 0.0;

float prevU = 0.0;

float dprev = 0.0;

const float Kih = 0.0;
float integrator = 0.0;

// Kalman filter
struct KalmanFilter kal;
float kalPinit = 0.1;
float kalQ = 0.1;
float kalR = 0.1;

// Variables
float measurements[4];
float u;
float vel;
float gyro_rate_bias;
float time;

// Function prototypes
int motPIDupdate(float error, float dt);
void calibrateGyro(int iterations);

task main() {

	writeDebugStreamLine("Started.");
	datalogClear();

	displayCenteredTextLine(0, "Initialising...");
	resetMotorEncoder(handlebarMotor);
	sleep(1000);

	// Get initial gyro bias
	calibrateGyro(10);
	measurements[2] = 0.0;

	// Initialise Kalman filter
	KalmanInit(&kal, gyro_rate_bias * DEG2RAD, kalPinit, kalQ, kalR);

	// Set motor speed
	setMotorSpeed(driveMotLeft, 100.0);
	setMotorSpeed(driveMotRight, 100.0);

	// Main loop
	time = 0.0;
	clearTimer(T1);
	while (true) {

		float dt = time1[T1] / 1000.0;
		if (dt >= T) {

			clearTimer(T1);

			// Get readings from sensors
			measurements[2] = ((-getGyroRate(gyroSensor) - gyro_rate_bias) * DEG2RAD); // PHIDOT
			measurements[0] = kal.x[0] + dt * measurements[2]; // PHI
			measurements[1] = getMotorEncoder(handlebarMotor); // DELTA
			measurements[3] = 0.0; // DELTADOT

			// Get lean angle estimate
			KalmanUpdate(&kal, measurements[0], measurements[2], dt);

			// Get control output
			integrator += Kih * (0 - measurements[0]);
			if (integrator > 5.0)
				integrator = 5.0;
			else if (integrator < -5.0)
				integrator = -5.0;


			hderiv = (2 * ((0 - measurements[2]) * RAD2DEG - (0 - prevmeas)) + hderivprev *(2 * tau - T)) / (2 * tau + T);
			prevmeas = measurements[2] * RAD2DEG;
			hderivprev = hderiv;

			u = Kph * (0 - measurements[0] * RAD2DEG) + Kdh * hderiv + integrator;

			u = updateTF(0 - measurements[0] * RAD2DEG);

			// Set motor
			float error = u - getMotorEncoder(handlebarMotor);

			int motctrl = motPIDupdate(error, dt);
			//setMotorSpeed(handlebarMotor, motctrl);

			setMotorSpeed(handlebarMotor, 5 * error - 0.1 * (getMotorRPM(handlebarMotor) + dprev) / 2.0);
			dprev = getMotorRPM(handlebarMotor);

		//	setMotorTarget(handlebarMotor, u - getMotorEncoder(handlebarMotor), 100);

			// Display information on screen
			displayCenteredTextLine(0, "Phi: %f", kal.x[0] * RAD2DEG);
			displayCenteredTextLine(1, "PhiDot: %f", measurements[2] * RAD2DEG);
			displayCenteredTextLine(2, "Delta: %f", measurements[1]);
			displayCenteredTextLine(3, "DeltaDot: %f", measurements[3]);
			displayCenteredTextLine(5, "Bias: %f", kal.x[1] * RAD2DEG);

			displayCenteredTextLine(7, "u: %f", u);
			displayCenteredTextLine(8, "motu: %d", getMotorEncoder(handlebarMotor));
			displayCenteredTextLine(9, "motctrl: %d", motctrl);

			time += dt;

			// Save to datalog
			int scale = 100;
			datalogDataGroupStart();
			datalogAddValue(0, getMotorRPM(driveMotLeft)); // Drive motor rpm
			datalogAddValue(1, getMotorEncoder(handlebarMotor)); // Steer angle
			datalogAddValue(2, (int) round(100 * measurements[0])); // Lean angle
			datalogAddValue(3, (int) round(100 * u)); // Controller output
			datalogDataGroupEnd();

		}

	}

	datalogClose();

}

int motPIDupdate(float error, float dt) {

	static float Kp = 5.0;
	static float Ki = 0.1;
	static float Kd = 0.5;
	static float tau = 0.1;

	static float integrator = 0.0;
	static float differentiator = 0.0;
	static float error_d1 = 0.0;

	integrator += (dt / 2) * (error + error_d1);
	differentiator = ((2 * tau - dt) / (2 * tau + dt)) * differentiator + (2 / (2 * tau + dt)) * (error - error_d1);

	error_d1 = error;

	float u_unsat = Kp * error + Ki * integrator + Kd * differentiator;

	float u = u_unsat;
	if (u > 10.0)
		u = 10.0;
	else if (u < -10.0)
		u = -10.0;

	// Anti-windup via back-calculation
	integrator += (dt / Ki) * (u - u_unsat);

	return ((int) -round(u));

}

// Calculate average gyro bias
void calibrateGyro(int iterations) {

	writeDebugStreamLine("Calibrating Gyro.");

	resetGyro(gyroSensor);
	sleep(3000);

	writeDebugStreamLine("Taking measurements...");

	measurements[0] = 0.0;
	gyro_rate_bias = 0.0;

	for (int i = 0; i < iterations; i++) {
		gyro_rate_bias += -getGyroRate(gyroSensor);
		delay(100);
	}

	writeDebugStreamLine("Iterations: %d | Bias sum: %f", iterations, gyro_rate_bias);
	gyro_rate_bias = gyro_rate_bias /(float) iterations;

}