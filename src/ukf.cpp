#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 2.85;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  is_initialized_ = false;
  n_x_ = 5;
  n_aug_ = n_x_ + 2;
  lambda_ = 3 - n_aug_;
  x_ = VectorXd(n_x_);
  P_ = MatrixXd::Identity(n_x_, n_x_);
  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_+1);
  weights_ = VectorXd(2*n_aug_+1);

  H_laser_ = MatrixXd(2,n_x_);
  H_laser_ << 1,0,0,0,0,
	      0,1,0,0,0;
  R_radar_ = MatrixXd(3,3);
  R_radar_ << std_radr_ * std_radr_, 0, 0,
	      0, std_radphi_ * std_radphi_, 0,
	      0, 0, std_radrd_ * std_radrd_;

  R_laser_ = MatrixXd(2,2);
  R_laser_ << std_laspx_ * std_laspx_, 0,
	      0, std_laspy_ * std_laspy_;

  weights_(0) = lambda_ / (lambda_ + n_aug_);
  for(int i = 1; i<(2 * n_aug_ + 1); i++)
	weights_(i) = 0.5 / (lambda_ + n_aug_);

  nis_radar_limit = 7.815;
  nis_laser_limit = 5.991;

  nis_radar_total = 0;
  nis_radar_true = 0;
  nis_laser_total = 0;
  nis_laser_true = 0;

}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
	if (is_initialized_ == false)
	{
		if (meas_package.sensor_type_ == meas_package.RADAR) {
			float rho = meas_package.raw_measurements_[0];
			float phi = meas_package.raw_measurements_[1];
			float rho_dot = meas_package.raw_measurements_[2];

			float px = rho * cos(phi);
			float py = rho * sin(phi);
			
			//state vector : [pos1 pos2 vel_abs yaw_angle yaw_rate]
			x_ << px, py, rho_dot, 0, 0;
		}
		else
		{
			float px = meas_package.raw_measurements_[0];
			float py = meas_package.raw_measurements_[1];

			//state vector : [pos1 pos2 vel_abs yaw_angle yaw_rate]
			x_ << px, py, 0, 0, 0;
		}

		time_us_ = meas_package.timestamp_;
		is_initialized_ = true;
		return;
	}
	float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
	time_us_ = meas_package.timestamp_;

	Prediction(dt);

	if (meas_package.sensor_type_ == meas_package.LASER && use_laser_ == true)
	{
		UpdateLidar(meas_package);
	}
	else if(meas_package.sensor_type_ == meas_package.RADAR && use_radar_ == true)
	{
		UpdateRadar(meas_package);
	}

	cout << "x_ = " << x_ << endl;
	cout << "P_ = " << P_ << endl;

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double dt) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

	//Prediction Step 1 - Generating sigma points
	VectorXd x_aug = VectorXd(n_aug_); //augmented states including deviations of yaw and longitudinal accelerations
	MatrixXd P_aug = MatrixXd(n_aug_, n_aug_); // augmented covariance matrix
	MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1); //augmented sigma matrix
	
	x_aug.head(n_x_) << x_;
	x_aug(n_x_) = 0;
	x_aug(n_x_ + 1) = 0;

	P_aug.fill(0.0);
	P_aug.topLeftCorner(n_x_, n_x_) << P_;
	P_aug(n_x_, n_x_) = std_a_ * std_a_;
	P_aug(n_x_ + 1, n_x_ + 1) = std_yawdd_ * std_yawdd_;

	MatrixXd L = MatrixXd(n_aug_, n_aug_);
	L = P_aug.llt().matrixL();

	Xsig_aug.col(0) = x_aug;
	for (int i = 0; i < n_aug_; i++)
	{
		Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
		Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
	}

	//Prediction Step 2 - Sigma point prediction
	float px, py, v, psi, psi_dot, nu_a, nu_yawdd; // state elements
	VectorXd state_dot = VectorXd(n_x_); //derivative of states
	VectorXd process_noise = VectorXd(n_x_); // process noise vector

	for (int i = 0; i<(2 * n_aug_ + 1); i++) {
		px = Xsig_aug(0, i);
		py = Xsig_aug(1, i);
		v = Xsig_aug(2, i);
		psi = Xsig_aug(3, i);
		psi_dot = Xsig_aug(4, i);
		nu_a = Xsig_aug(5, i);
		nu_yawdd = Xsig_aug(6, i);

		process_noise << 0.5*dt*dt*cos(psi)*nu_a,
			0.5*dt*dt*sin(psi)*nu_a,
			dt*nu_a,
			0.5*dt*dt*nu_yawdd,
			dt*nu_yawdd;

		if (fabs(psi_dot) < 0.0001) {
			state_dot << v*cos(psi)*dt, v*sin(psi)*dt, 0, 0, 0;
		}
		else {
			state_dot << v / psi_dot*(sin(psi + psi_dot*dt) - sin(psi)),
				v / psi_dot*(-cos(psi + psi_dot*dt) + cos(psi)),
				0, psi_dot*dt, 0;
		}

		Xsig_pred_.col(i) = Xsig_aug.col(i).head(n_x_) + state_dot + process_noise;

	}

	//Prediction Step 3 - Calculating the predicted mean and covariance
	VectorXd xdiff = VectorXd(n_x_);
	x_.fill(0.0);
	P_.fill(0.0);

	for (int i = 0; i<(2 * n_aug_ + 1); i++)
		x_ += weights_(i) * Xsig_pred_.col(i);

	for (int i = 0; i<(2 * n_aug_ + 1); i++) {
		xdiff = Xsig_pred_.col(i) - x_;

		while (xdiff(3) > M_PI) xdiff(3) -= 2 * M_PI;
		while (xdiff(3) < -M_PI) xdiff(3) += 2 * M_PI;

		P_ += weights_(i) * xdiff * xdiff.transpose();

	}

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
	//We can use Kalman equations here because Lidar measurements are lineer
	VectorXd z_pred = H_laser_ * x_;
	VectorXd z = VectorXd(2);
	z << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1];
	VectorXd y = z - z_pred; //2x1
	MatrixXd Ht = H_laser_.transpose(); // 5x2
	MatrixXd PHt = P_ * Ht; // 5x2
	MatrixXd S = H_laser_ * PHt + R_laser_; // 2x2
	MatrixXd Si = S.inverse(); // 2x2
	MatrixXd K = PHt * Si; // 5x2

	//new estimate
	x_ = x_ + (K * y); // 5x1
	long x_size = x_.size();
	MatrixXd I = MatrixXd::Identity(x_size, x_size);
	P_ = (I - K * H_laser_) * P_;

 	//Step 3 - Calculate the NIS value
	nis_laser_total += 1;
	float epsilon = y.transpose() * Si * y;
	if (epsilon < nis_laser_limit){
		nis_laser_true += 1;
		std::cout << "How accurate is the Laser Measurement? " << nis_laser_true/nis_laser_total << std::endl;
	}

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

	//Update step 1 - Predict measurement
	//set measurement dimension, radar can measure r, phi, and r_dot
	int n_z = 3;
	//create matrix for sigma points in measurement space
	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);

	//measurement covariance matrix S
	MatrixXd S = MatrixXd(n_z, n_z);
	float rho, theta, rho_dot, px, py, v, psi, psi_dot;
	MatrixXd R = MatrixXd(n_z, n_z);
	VectorXd zdiff = VectorXd(n_z);

	z_pred.fill(0.0);
	S.fill(0.0);

	for (int i = 0; i<(2 * n_aug_ + 1); i++) {
		px = Xsig_pred_(0, i);
		py = Xsig_pred_(1, i);
		v = Xsig_pred_(2, i);
		psi = Xsig_pred_(3, i);
		psi_dot = Xsig_pred_(4, i);

		rho = sqrt(px*px + py*py);
		theta = atan2(py, px);

		if (fabs(rho) < 0.0001)
			rho_dot = 0;
		else
			rho_dot = v*(px*cos(psi) + py*sin(psi)) / rho;

		Zsig(0,i) = rho;
		Zsig(1,i) = theta;
		Zsig(2,i) = rho_dot;

		z_pred += weights_(i) * Zsig.col(i);
	}

	R = R_radar_;

	for (int i = 0; i<(2 * n_aug_ + 1); i++) {
		zdiff = Zsig.col(i) - z_pred;
		while (zdiff(1) < -M_PI) zdiff(1) += 2 * M_PI;
		while (zdiff(1) > M_PI) zdiff(1) -= 2 * M_PI;

		S += weights_(i)*zdiff*zdiff.transpose();
	}

	S += R;


	//Update Step 2 - Update state
	MatrixXd Tc = MatrixXd(n_x_, n_z);
	VectorXd xdiff = VectorXd(n_x_);
	VectorXd z = VectorXd(n_z);
	MatrixXd K = MatrixXd(n_x_, n_z);
	Tc.fill(0.0);

	for (int i = 0; i<(2 * n_aug_ + 1); i++) {
		xdiff = Xsig_pred_.col(i) - x_;
		zdiff = Zsig.col(i) - z_pred;

		while (xdiff(3) > M_PI) xdiff(3) -= 2.*M_PI;
		while (xdiff(3) < -M_PI) xdiff(3) += 2.*M_PI;

		while (zdiff(1) > M_PI) zdiff(1) -= 2.*M_PI;
		while (zdiff(1) < -M_PI) zdiff(1) += 2.*M_PI;

		Tc += weights_(i)*xdiff*zdiff.transpose();
	}

	MatrixXd S_inv = S.inverse();
	K = Tc*S_inv;

	z << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), meas_package.raw_measurements_(2);
	zdiff = z - z_pred;

	while (zdiff(1) > M_PI) zdiff(1) -= 2.*M_PI;
	while (zdiff(1) < -M_PI) zdiff(1) += 2.*M_PI;

	x_ = x_ + K*zdiff;
	P_ = P_ - K*S*K.transpose();

	//Step 3 - Calculate the NIS value
	nis_radar_total += 1;
	float epsilon = zdiff.transpose() * S_inv * zdiff;
	if (epsilon < nis_radar_limit){
		nis_radar_true += 1;
		std::cout << "How accurate is the Radar Measurement? " << nis_radar_true / nis_radar_total << std::endl;
	}

}
