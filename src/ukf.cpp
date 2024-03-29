#include<iostream>
#include "ukf.h"
#include "Eigen/Dense"

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
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
  std_a_ = 2.0;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.5;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

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
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */

  //Init flag for first measurement
  is_initialized_ = false;

  //state dimention 
  n_x_ = 5;

  // Augmented state dimension
  n_aug_ = 7;

  // augmented sigma points 
  Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  //predited sigma points 
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

  // Define spreading parameter
  lambda_ = 3 - n_aug_;

    //init weights 
  weights_ = VectorXd(2*n_aug_+1);

  // Start time
  time_us_ = 0;

  //NIS calc Radar
  NIS_radar = 0.00;

  //NIS calc Radar
  NIS_lidar = 0.00;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */

  //first measument for initialisation of state and covariance matrices
  if(!is_initialized_)
  {
    if(meas_package.sensor_type_ == MeasurementPackage::LASER)
    {
      
      double x = meas_package.raw_measurements_(0);
      double y = meas_package.raw_measurements_(1);

      x_ << x,y,0,0,0 ;

      P_ << std_laspx_*std_laspx_, 0, 0, 0, 0,
            0,std_laspy_*std_laspx_,0,0,0,
            0,0,1,0,0,
            0,0,0,1,0,
            0,0,0,0,1;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
    {
      
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rho_dot = meas_package.raw_measurements_(2);

      double x = rho*sin(phi);
      double y = rho*cos(phi);

      x_ << x,y,rho_dot,phi,0;

      P_ << std_radr_*std_radr_,0,0,0,0,
      0,std_radr_*std_radr_,0,0,0,
      0,0,std_radr_*std_radr_,0,0,
      0,0,0,std_radphi_*std_radphi_,0,
      0,0,0,0,1;
    }

    is_initialized_ = true;
    time_us_ = meas_package.timestamp_;
    return ;
  }
  
  double dt = (meas_package.timestamp_ - time_us_)/1000000.0;
  time_us_ = meas_package.timestamp_;

  Prediction(dt);

  if(meas_package.sensor_type_==MeasurementPackage::LASER)
  {
    UpdateLidar(meas_package);
  }
  else if(meas_package.sensor_type_==MeasurementPackage::RADAR)
  {
    UpdateRadar(meas_package);
  }

}

void UKF::Prediction(double delta_t) {
  
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */


  //************ 1 .Sigma Point generation **********//
    // create augmented mean vector
    VectorXd x_aug = VectorXd(n_aug_); 

    // create augmented state covariance
    MatrixXd P_aug = MatrixXd(n_aug_, n_aug_); 

    // create augmented mean state
    x_aug.head(5) = x_ ;
    x_aug(5) = 0 ;
    x_aug(6) = 0 ;

    // create augmented covariance matrix
    P_aug.fill(0.0) ; 
    P_aug.topLeftCorner(5,5) = P_;
    P_aug(5,5) = std_a_*std_a_;
    P_aug(6,6) = std_yawdd_*std_yawdd_;

    // create square root matrix
    MatrixXd A = P_aug.llt().matrixL();


    // create augmented sigma points
          //1st pt of Sigma pt matrix 
    Xsig_aug_.col(0) = x_aug ; 
      //next pts 
    for(int i = 0 ; i < n_aug_ ; ++i)
    {
      Xsig_aug_.col(i+1) = x_aug + sqrt(lambda_ + n_aug_)*A.col(i) ;
      Xsig_aug_.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_)*A.col(i) ;
    }

  
  //************ 2 .Predict sigma points  **********//
    
    for(int i = 0 ; i < 2*n_aug_+ 1 ; ++i)
    {
      //extract all values
      double px = Xsig_aug_(0,i) ;
      double py = Xsig_aug_(1,i) ;
      double v = Xsig_aug_(2,i) ;
      double yaw = Xsig_aug_(3,i) ;
      double yawd = Xsig_aug_(4,i) ;
      double nu_a = Xsig_aug_(5,i) ;
      double nu_yawdd = Xsig_aug_(6,i) ;

      //predicted state values 
      double px_p , py_p ;
      // avoid division by zero
      if(fabs(yawd) > 0.001)
      {
          px_p = px + v/yawd * (sin(yaw + yawd * delta_t) - sin(yaw)) ; 
          py_p = py + v/yawd * (-cos(yaw + yawd * delta_t) + cos(yaw)) ; 
      }
      else
      {
          px_p = px + v*delta_t*cos(yaw) ; 
          py_p = py + v*delta_t*sin(yaw) ; 
      }
      
      double v_p = v ;
      double yaw_p = yaw + yawd*delta_t;
      double yawd_p = yawd ; 

    //add noise
    px_p = px_p + 0.5 *delta_t*delta_t * nu_a*cos(yaw) ; 
    py_p = py_p + 0.5*delta_t*delta_t * nu_a*sin(yaw) ;
    v_p = v_p + delta_t*nu_a;
    yaw_p = yaw_p + 0.5*delta_t*delta_t*nu_yawdd;
    yawd_p = yawd_p + delta_t*nu_yawdd ; 

    // write predicted sigma points into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
    }
  
  //************ 3 .Predict mean and covariance  **********//
  // set weights
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;

  for(int i=1 ; i<2*n_aug_+1;++i)
  {
    weights_(i) = 0.5/(lambda_+n_aug_) ;
  }

  // predict state mean
  x_.fill(0.0) ; 
  for(int i=0 ; i<2*n_aug_+1;++i)
  {
    x_ = x_ + weights_(i)*Xsig_pred_.col(i);
  }

  // predict state covariance matrix
  P_.fill(0.00);
  for(int i=0 ; i<2*n_aug_+1;++i)
  {
    //state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_ ;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ; 
  }


}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
    /**
     * TODO: Complete this function! Use lidar data to update the belief 
     * about the object's position. Modify the state vector, x_, and 
     * covariance, P_.
     * You can also calculate the lidar NIS, if desired.
     */
      //setting measurment dimention 
      int n_z = 2;
      //create matrix for sigma points in measument space
      MatrixXd Zsig = MatrixXd(n_z,2*n_aug_+1);
      // mean predicted measurement
      VectorXd z_pred = VectorXd(n_z);
      // measurement covariance matrix S
      MatrixXd S = MatrixXd(n_z,n_z);

  //************1.  transform sigma points into measurement space ***********//
    for(int i = 0 ; i < 2*n_aug_+1 ; ++i)
    {
      //extract values 
      double px = Xsig_pred_(0,i);
      double py = Xsig_pred_(1,i);

      //through measurement model --> sigma points in measurement space
      Zsig(0,i) = px ;                                     //px
      Zsig(1,i) = py;                                      //py

    }
  //************2. calculate mean predicted measurement ***********//
    z_pred.fill(0.00);
    for(int i = 0 ; i < 2*n_aug_+1 ; ++i)
    {
      z_pred = z_pred + weights_(i)*Zsig.col(i);
    }
    
    // calculate innovation covariance matrix S
    S.fill(0.00);
    for(int i=0 ; i<2*n_aug_+1;++i)
    {
      // difference
      VectorXd z_pred_diff = Zsig.col(i) - z_pred ;

      S = S + weights_(i) * z_pred_diff * z_pred_diff.transpose() ; 

    }

    //add noise R (measurment noise covariance matrix)
    MatrixXd R = MatrixXd(n_z,n_z);
    R << std_laspx_ * std_laspx_,  0,
         0, std_laspy_ * std_laspy_;
    
    S = S + R ;

  //************3. Update measurment ***********//
    // create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z);
    // calculate cross correlation matrix
    Tc.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
      // residual
      VectorXd z_diff = Zsig.col(i) - z_pred;
      // angle normalization
      while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
      while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

      // state difference
      VectorXd x_diff = Xsig_pred_.col(i) - x_;
      // angle normalization
      while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
      while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

      Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
    }

    // Kalman gain K;
    MatrixXd K = Tc * S.inverse();

    //incoming radar measurement
    VectorXd z = VectorXd(n_z);
    z=meas_package.raw_measurements_;

    // residual
    VectorXd z_diff = z - z_pred;

    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // update state mean and covariance matrix
    x_ = x_ + K * z_diff;
    P_ = P_ - K*S*K.transpose();

    //calc NIS for lidar
    NIS_lidar = z_diff.transpose()*S.inverse()*z_diff;

    std::cout<<"NIS Lidar : "<<NIS_lidar<<std::endl;
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
    /**
     * TODO: Complete this function! Use radar data to update the belief 
     * about the object's position. Modify the state vector, x_, and 
     * covariance, P_.
     * You can also calculate the radar NIS, if desired.
     */

      //setting measurment dimention 
      int n_z = 3;
      //create matrix for sigma points in measument space
      MatrixXd Zsig = MatrixXd(n_z,2*n_aug_+1);
      // mean predicted measurement
      VectorXd z_pred = VectorXd(n_z);
      // measurement covariance matrix S
      MatrixXd S = MatrixXd(n_z,n_z);

  //************1.  transform sigma points into measurement space ***********//
    for(int i = 0 ; i < 2*n_aug_+1 ; ++i)
    {
      //extract values 
      double px = Xsig_pred_(0,i);
      double py = Xsig_pred_(1,i);
      double v = Xsig_pred_(2,i);
      double yaw = Xsig_pred_(3,i);

      //through measurement model --> sigma points in measurement space
      Zsig(0,i) = sqrt(px*px + py*py) ;                                     //r
      Zsig(1,i) = atan2(py,px);                                             //phi
      Zsig(2,i) =( px*v*cos(yaw) + py*v*sin(yaw) )/ sqrt(px*px + py*py);    //r_dot
    }
    
  //************2. calculate mean predicted measurement ***********//
    z_pred.fill(0.00);
    for(int i = 0 ; i < 2*n_aug_+1 ; ++i)
    {
      z_pred = z_pred + weights_(i)*Zsig.col(i);
    }
    
    // calculate innovation covariance matrix S
    S.fill(0.00);
    for(int i=0 ; i<2*n_aug_+1;++i)
    {
      // difference
      VectorXd z_pred_diff = Zsig.col(i) - z_pred ;

      S = S + weights_(i) * z_pred_diff * z_pred_diff.transpose() ; 

    }

    //add noise R (measurment noise covariance matrix)
    MatrixXd R = MatrixXd(n_z,n_z);
    R << std_radr_*std_radr_ , 0 , 0 ,
        0,std_radphi_*std_radphi_ , 0,
        0,0,std_radrd_*std_radrd_;
    
    S = S + R ;

  //************3. Update measurment ***********//
  // create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  // calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //incoming radar measurement
  VectorXd z = VectorXd(n_z);
  z=meas_package.raw_measurements_;

  // residual
  VectorXd z_diff = z - z_pred;

  // angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  // update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  //calc NIS for lidar
  NIS_radar = z_diff.transpose()*S.inverse()*z_diff;

  std::cout<<"NIS Radar : "<<NIS_radar<<std::endl;
}