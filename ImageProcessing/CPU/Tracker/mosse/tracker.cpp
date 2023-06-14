#include "tracker.h"

Tracker::Tracker()
{
    m_initTrack = false;
    m_state = TRACK_INVISION;
    m_PSRMask = 11;
    m_PSRRatio[0] = 2;
    m_PSRRatio[1] = 5;

    m_learningRate = 0.125;

    m_imgSize.width = 0;
    m_imgSize.height = 0;
    m_haveEps = true;
}

Tracker::~Tracker() {}

void Tracker::initTrack(const cv::Mat &input_image, cv::Rect input_rect)
{
    if(this->m_initTrack)
        return;
    if(input_rect.width < 5 || input_rect.height < 5){
        return;
    }
    if(input_image.cols < 5 || input_image.rows < 5){
        std::cerr << "Init Image too small\r\n";
        return;
    }
    this->m_imgSize.width = input_image.cols;
    this->m_imgSize.height = input_image.rows;

    m_trackSize = std::max(input_rect.width, input_rect.height);

    if(m_trackSize < 200)
        m_stdSize = m_trackSize;
    else
    {
        m_stdSize = m_trackSize / 2;
    }
    if(input_rect.x + input_rect.width >= input_image.cols ||
            input_rect.y + input_rect.height >= input_image.rows){
        std::cerr << "Init Current ROI out of input\r\n";
        return;
    }
    if(m_stdSize < 5 ){
        std::cerr << "Standard size too small\r\n";
        return;
    }
    cv::Mat temp = input_image(input_rect).clone();
    cv::resize(temp, temp, cv::Size(m_stdSize, m_stdSize));
    temp.copyTo(m_prevImg.real_image);
//    input_image(input_rect).copyTo(m_prevImg.real_image);
    m_prevImg.cols = m_prevImg.real_image.cols;
    m_prevImg.rows = m_prevImg.real_image.rows;
    std::cout << "=====================================\r\n";

    // Extract HoG
//    m_prevImg.hog_feature = extractFeatures(m_prevImg);
//    std::cout << m_prevImg.hog_feature.size() << std::endl;

//    m_featureMapSize[0] = int(std::sqrt(m_prevImg.hog_feature.cols));
//    m_featureMapSize[1] = m_featureMapSize[0];
//    m_featureMapSize[2] = m_prevImg.hog_feature.rows;

//    createHannWindow(m_hanWin, m_featureMapSize);
//    std::cout<< m_hanWin.size() << std::endl;
//    m_prevImg.hog_feature = m_hanWin.mul(m_prevImg.hog_feature);

    ComputeDFT(m_prevImg, true);
    setROI(input_rect);
    initFilter();
    this->m_initTrack = true;
}

void Tracker::performTrack(const cv::Mat &input_image)
{
    if(!this->m_initTrack)
        return;
    if(this->m_filter.empty())
    {
        std::cerr << "Must initialize filter in first!\r\n";
        return;
    }
    if(input_image.cols < 5 || input_image.rows < 5){
        std::cerr << "Image too small\r\n";
        return;
    }
    if(this->m_currRoi.ROI.x + this->m_currRoi.ROI.width >= input_image.cols ||
            this->m_currRoi.ROI.y + this->m_currRoi.ROI.height >= input_image.rows){
        std::cerr << "Current ROI out of input\r\n";
        return;
    }
    if(m_stdSize < 5 ){
        std::cerr << "Standard size too small\r\n";
        return;
    }
    cv::Point newLoc;
    cv::Mat temp = input_image(this->m_currRoi.ROI).clone();    
    cv::resize(temp, temp, cv::Size(m_stdSize, m_stdSize));
    temp.copyTo(m_currImg.real_image);
//    input_image(this->m_currRoi.ROI).copyTo(this->m_currImg.real_image);
    ComputeDFT(m_currImg, true);

    newLoc = PerformTrack();                                                   //Perform tracking
    newLoc *= (m_trackSize / m_stdSize);
//    std::cout << "New Location: " << newLoc << std::endl;

    if (newLoc.x >= 0 && newLoc.y >= 0)                                            //If PSR > ratio then update
    {
        this->m_state = TRACK_INVISION;
        update(newLoc);                                                        //Update Tracker
    }
    else this->m_state = TRACK_OCCLUDED;
}

void Tracker::resetTrack()
{
    this->m_initTrack = false;
}

cv::Rect Tracker::getPosition() const       //Get ROI position
{
    return m_currRoi.ROI;
}

int Tracker::getState()
{
    return m_state;
}
bool Tracker::isInitialized(){
    return m_initTrack;
}
void Tracker::ComputeDFT(image_track &input_image, bool preprocess)
{
    cv::Mat res = this->ComputeDFT(input_image.real_image, preprocess);
    input_image.image_spectrum = res;
    input_image.opti_dft_comp_rows = res.rows;
    input_image.opti_dft_comp_cols = res.cols;
}

cv::Mat Tracker::ComputeDFT(const cv::Mat &input_image, bool preprocess)
{
    cv::Mat gray_padded, complexI;

    int w = input_image.rows;
    int h = input_image.cols;

    //Get optimal dft image size
    int i = cv::getOptimalDFTSize(w);
    int j = cv::getOptimalDFTSize(h);

    //Zero pad input image up to optimal dft size
    cv::copyMakeBorder(input_image, gray_padded, 0, i - w, 0, j - h, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    input_image.copyTo(gray_padded);

    gray_padded.convertTo(gray_padded, CV_32F);

    if(preprocess)
    {
        cv::normalize(gray_padded, gray_padded, 0.0, 1.0, cv::NORM_MINMAX);

        gray_padded += cv::Scalar::all(1);
        cv::log(gray_padded, gray_padded);

        cv::Scalar mean, stddev;
        cv::meanStdDev(gray_padded, mean, stddev);
        gray_padded -= mean.val[0];

        cv::Mat tmp;
        cv::multiply(gray_padded, gray_padded, tmp);
        cv::Scalar sum_ = cv::sum(tmp);
        gray_padded /= sum_.val[0];

        //Apply Hanning window to reduce image boundaries effect
        if (this->m_hanningWindow.empty() || gray_padded.size() != this->m_hanningWindow.size())
        {
            cv::Mat hanningWin_;
            cv::createHanningWindow(hanningWin_, gray_padded.size(), CV_32F);
            hanningWin_.copyTo(this->m_hanningWindow);
        }
        cv::multiply(gray_padded, this->m_hanningWindow, gray_padded);
    }

    dft(gray_padded, complexI, cv::DFT_COMPLEX_OUTPUT);    //Compute Direct Fourier Transform

    //Crop the spectrum, if it has an odd number of rows or columns
    cv::Rect cropRect = cv::Rect(0, 0, complexI.cols & -2, complexI.rows & -2);
    if(cropRect.width > complexI.cols){
        cropRect.width = complexI.cols;
    }
    if(cropRect.height > complexI.rows){
        cropRect.height = complexI.rows;
    }
    complexI = complexI(cropRect);
    return complexI;
}

bool Tracker::getInitStatus()
{
    return m_initTrack;
}

void Tracker::setROI(cv::Rect input_roi)
{
    //Init ROI position and center
    this->m_currRoi.ROI = input_roi;
    this->m_currRoi.ROI_center.x = round(input_roi.width / 2);
    this->m_currRoi.ROI_center.y = round(input_roi.height / 2);
}

void Tracker::initFilter()
{
    cv::Mat affine_G, affine_image, temp_image_dft, temp_desi_G, filter;
    cv::Mat temp_FG, temp_FF, num, dem, eps;

    //Number of images to init filter
    int N = 8;

    //Create the the desired output - 2D Gaussian
    cv::Mat Mask_gauss = cv::Mat::zeros(this->m_prevImg.real_image.size(), CV_32F);
//    maskDesiredG(Mask_gauss, round(this->m_currRoi.ROI.width / 2),
//                 round(this->m_currRoi.ROI.height / 2));
    maskDesiredG(Mask_gauss, round(m_stdSize / 2), round(m_stdSize / 2));
    temp_FG = cv::Mat::zeros(this->m_prevImg.opti_dft_comp_rows, this->m_prevImg.opti_dft_comp_cols, this->m_prevImg.image_spectrum.type());
    temp_FF = cv::Mat::zeros(this->m_prevImg.opti_dft_comp_rows, this->m_prevImg.opti_dft_comp_cols, this->m_prevImg.image_spectrum.type());

    temp_image_dft = this->m_prevImg.image_spectrum;
    temp_desi_G = ComputeDFT(Mask_gauss, false);

    temp_desi_G.copyTo(this->m_prevImg.filter_output);

    mulSpectrums(temp_desi_G, temp_image_dft, num, 0, true);       //Element-wise spectrums multiplication G o F*
    temp_FG += num;

    mulSpectrums(temp_image_dft, temp_image_dft, dem, 0, true);     //Element-wise spectrums multiplication F o F*
    temp_FF += dem;

    if (m_haveEps)
    {
        //Regularization parameter
        eps = createEps(dem);
        dem += eps;
        temp_FF += dem;
    }

    srand(time(NULL));

    for (int i = 0; i<(N - 1); i++)
    {//Create image dataset with input image affine transforms

        affineTransform(Mask_gauss, this->m_prevImg.real_image, affine_G, affine_image);    //Input image and desired output affine transform
        temp_image_dft = ComputeDFT(affine_image, true);        //Affine image DFT
        temp_desi_G = ComputeDFT(affine_G, false);              //Affine output DFT

        mulSpectrums(temp_desi_G, temp_image_dft, num, 0, true);   //Element-wise spectrums multiplication G o F*
        temp_FG += num;

        mulSpectrums(temp_image_dft, temp_image_dft, dem, 0, true); //Element-wise spectrums multiplication F o F*

        if (m_haveEps)
        {
            eps = createEps(dem);
            dem += eps;
        }

        temp_FF += dem;
    }

    dftDiv(temp_FG, temp_FF, filter);       //Element-wise spectrum Division

    filter.copyTo(this->m_filter);           //Filter
}

void Tracker::maskDesiredG(cv::Mat &output, int u_x, int u_y, double sigma, bool norm_energy)
{
    sigma *= sigma;

    //Fill input matrix as 2D Gaussian
    for (int i = 0; i < output.rows; i++)
    {
        for (int j = 0; j < output.cols; j++)
        {
            output.at<float>(i, j) = 255 * exp((-(i - u_y)*(i - u_y) / (2 * sigma)) +
                                               (-(j - u_x)*(j - u_x) / (2 * sigma)));
        }
    }

    if (norm_energy)    //If true, norm image energy so that it sum up to 1
    {
        cv::Scalar sum_;
        sum_ = sum(output);
        output /= sum_.val[0];
    }
}

cv::Mat Tracker::createEps(const cv::Mat &input_, double std)
{//Compute regularization parameter for a given input matrix

    //Compute input matrix mean and std
    cv::Scalar mean, stddev;
    cv::meanStdDev(input_, mean, stddev);

    cv::Mat eps = cv::Mat::zeros(input_.size(), input_.type());

    //Fill output matrix so that white noise zero mean and std a fraction of input matrix mean value
    cv::randn(eps, 0, std*(mean.val[0]));

    //Set imaginary part of noise to all zeros
    for (int x = 0; x<eps.rows; x++)
    {
        for (int y = 0; y<eps.cols; y++)
        {
            eps.at<cv::Vec2f>(x, y)[1] = 0;
        }
    }

    eps.at<cv::Vec2f>(0, 0)[0] = 0;
    eps.at<cv::Vec2f>(input_.rows - 1, 0)[0] = 0;
    eps.at<cv::Vec2f>(0, input_.cols - 1)[0] = 0;
    eps.at<cv::Vec2f>(input_.rows - 1, input_.cols - 1)[0] = 0;

    return eps;
}

void Tracker::affineTransform(const cv::Mat &input_image, const cv::Mat &input_image2, cv::Mat &aff_img, cv::Mat &aff_img2)
{//Apply same randomly defined affine transform to both input matrice

    if (input_image.size() != input_image2.size())
    {
        std::cout << "Error while computing affine transform !" << std::endl;
        return;
    }

    //output images
    aff_img = cv::Mat::zeros(input_image.rows, input_image.cols, input_image.type());
    aff_img2 = cv::Mat::zeros(input_image2.rows, input_image2.cols, input_image2.type());

    int cols = input_image.cols;
    int rows = input_image.rows;

    cv::Point2f input_pts[3];
    cv::Point2f output_pts[3];

    float pts0_r, pts0_c, pts1_r, pts1_c, pts2_r, pts2_c;

    cv::Mat affine_tr(2, 3, CV_32FC1);

    input_pts[0] = cv::Point2f(0, 0);
    input_pts[1] = cv::Point2f(cols - 1, 0);
    input_pts[2] = cv::Point2f(0, rows - 1);

    //Define affine transform 'intensity'
    pts0_r = rand() % 5; pts0_r /= 100;
    pts0_c = rand() % 5; pts0_c /= 100;

    pts1_r = rand() % 5; pts1_r /= 100;
    pts1_c = rand() % 5 + 95; pts1_c /= 100;

    pts2_r = rand() % 5 + 95; pts2_r /= 100;
    pts2_c = rand() % 5; pts2_c /= 100;

    output_pts[0] = cv::Point2f(cols*pts0_c, rows*pts0_r);
    output_pts[1] = cv::Point2f(cols*pts1_c, rows*pts1_r);
    output_pts[2] = cv::Point2f(cols*pts2_c, rows*pts2_r);

    affine_tr = cv::getAffineTransform(input_pts, output_pts);        //Get transformation matrix
    if(affine_tr.cols < 3 || affine_tr.rows < 2){
        affine_tr = cv::Mat(2,3,CV_32FC1,cv::Scalar::all(0));
        affine_tr.at<float>(0,0) = 1;
        affine_tr.at<float>(1,1) = 1;
    }
    cv::warpAffine(input_image, aff_img, affine_tr, aff_img.size());  //Apply transformation matrix
    cv::warpAffine(input_image2, aff_img2, affine_tr, aff_img2.size());
}

void Tracker::dftDiv(const cv::Mat &dft_a, const cv::Mat &dft_b, cv::Mat &output_dft)
{//Compute complex divison

    assert(dft_a.size() == dft_b.size() && dft_a.type() == dft_b.type() &&
           dft_a.channels() == dft_b.channels() && dft_a.channels() == 2);

    cv::Mat out_temp = cv::Mat::zeros(dft_a.rows, dft_a.cols, dft_a.type());

    for (int x = 0; x<dft_a.rows; x++)
    {
        for (int y = 0; y<dft_a.cols; y++)
        {
            out_temp.at<cv::Vec2f>(x, y)[0] = ((dft_a.at<cv::Vec2f>(x, y)[0] * dft_b.at<cv::Vec2f>(x, y)[0]) +
                    (dft_a.at<cv::Vec2f>(x, y)[1] * dft_b.at<cv::Vec2f>(x, y)[1])) /
                    ((dft_b.at<cv::Vec2f>(x, y)[0] * dft_b.at<cv::Vec2f>(x, y)[0]) +
                    (dft_b.at<cv::Vec2f>(x, y)[1] * dft_b.at<cv::Vec2f>(x, y)[1]));

            out_temp.at<cv::Vec2f>(x, y)[1] = ((dft_a.at<cv::Vec2f>(x, y)[1] * dft_b.at<cv::Vec2f>(x, y)[0]) -
                    (dft_a.at<cv::Vec2f>(x, y)[0] * dft_b.at<cv::Vec2f>(x, y)[1])) /
                    ((dft_b.at<cv::Vec2f>(x, y)[0] * dft_b.at<cv::Vec2f>(x, y)[0]) +
                    (dft_b.at<cv::Vec2f>(x, y)[1] * dft_b.at<cv::Vec2f>(x, y)[1]));
        }
    }

    out_temp.copyTo(output_dft);
}

cv::Point Tracker::PerformTrack()
{
    cv::Mat mat_correlation, idft_correlation;
    float PSR_val;
    cv::Point maxLoc;

    //Element-wise matrice multiplication, second arg is complex conjugate H*
    cv::mulSpectrums(m_currImg.image_spectrum, m_filter, mat_correlation, 0, false);

    //Inverse DFT real output
    dft(mat_correlation, idft_correlation, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT);

    // Correlation image
    cv::Mat corrImg;
    normalize(idft_correlation, idft_correlation, 0.0, 255.0, cv::NORM_MINMAX);

    // Filter image
    cv::Mat filtImg;
    dft(m_filter, filtImg, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT);

    // Shifting the image!!
    int cx = filtImg.cols / 2;
    int cy = filtImg.rows / 2;
    cv::Mat q0(filtImg, cv::Rect(0, 0, cx, cy));   // Top-Left - Create a ROI per quadrant
    cv::Mat q1(filtImg, cv::Rect(cx, 0, cx, cy));  // Top-Right
    cv::Mat q2(filtImg, cv::Rect(0, cy, cx, cy));  // Bottom-Left
    cv::Mat q3(filtImg, cv::Rect(cx, cy, cx, cy)); // Bottom-Right

    cv::Mat tmp;                           // swap quadrants (Top-Left with Bottom-Right)
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);
    q1.copyTo(tmp);                    // swap quadrant (Top-Right with Bottom-Left)
    q2.copyTo(q1);
    tmp.copyTo(q2);

    normalize(filtImg, filtImg, 0.0, 255.0, cv::NORM_MINMAX);
    flip(filtImg, filtImg, 0);

    PSR_val = computePSR(idft_correlation);  //Compute PSR

    if (PSR_val >= m_PSRRatio[1])    //Get new pos if object detected
    {
        minMaxLoc(idft_correlation, NULL, NULL, NULL, &maxLoc);
//        std::cout << "Max Location: " << maxLoc << std::endl;
        // ******* Trying to keep a nice and clean filter output image ***********************
        cv::Mat new_output = cv::Mat::zeros(mat_correlation.size(), CV_32F);

        maskDesiredG(new_output, maxLoc.x, maxLoc.y);

        new_output = ComputeDFT(new_output, false);
        // ***********************************************************************************

        new_output.copyTo(this->m_currImg.filter_output);
    }
    else if (PSR_val > m_PSRRatio[0])
    { //Return -1 coordinates if object occluded
        maxLoc.x = -2;
        maxLoc.y = -2;
    }
    else
    { //Return -2 coordinates if object lost
        maxLoc.x = -10;
        maxLoc.y = -10;
    }
    return maxLoc;
}

float Tracker::computePSR(const cv::Mat &correlation_mat)
{//Compute Peak-to-Sidelobe Ratio
//    printf("Start calculating PSR...\r\n");
    double max_val = 0;
    cv::Point max_loc;
    cv::Mat PSR_mask = cv::Mat::ones(correlation_mat.rows, correlation_mat.cols, CV_8U);
    cv::Scalar mean, stddev;

    minMaxLoc(correlation_mat, NULL, &max_val, NULL, &max_loc);     //Get location of max arg

    //Define PSR mask
    int win_size = floor(this->m_PSRMask / 2);
    cv::Rect mini_roi = cv::Rect(std::max(max_loc.x - win_size, 0), std::max(max_loc.y - win_size, 0), this->m_PSRMask, this->m_PSRMask);

    //Handle image boundaries
    if ((mini_roi.x + mini_roi.width) > PSR_mask.cols)
    {
        mini_roi.width = PSR_mask.cols - mini_roi.x;
    }
    if ((mini_roi.y + mini_roi.height) > PSR_mask.rows)
    {
        mini_roi.height = PSR_mask.rows - mini_roi.y;
    }

    cv::Mat temp = PSR_mask(mini_roi);
    temp *= 0;
    meanStdDev(correlation_mat, mean, stddev, PSR_mask);   //Compute matrix mean and std

    return (max_val - mean.val[0]) / stddev.val[0];     //Compute PSR
}

void Tracker::update(cv::Point new_location)
{//Update Tracker
    updateFilter();                         //Update filter
    this->m_prevImg = this->m_currImg;     //Update frame
    updateRoi(new_location, false);          //Update ROI position
}

void Tracker::updateFilter()
{
    cv::Mat Ai_curr, Bi_curr, Ai_prev, Bi_prev, A, B, filter, eps_curr, eps_prev;
    cv::mulSpectrums(this->m_currImg.filter_output, this->m_currImg.image_spectrum, Ai_curr, 0, true);      //Element-wise spectrums multiplication G o F*
    cv::mulSpectrums(this->m_prevImg.filter_output, this->m_prevImg.image_spectrum, Ai_prev, 0, true);          //Element-wise spectrums multiplication G-1 o F-1*

    cv::mulSpectrums(this->m_currImg.image_spectrum, this->m_currImg.image_spectrum, Bi_curr, 0, true);     //Element-wise spectrums multiplication F o F*
    cv::mulSpectrums(this->m_prevImg.image_spectrum, this->m_prevImg.image_spectrum, Bi_prev, 0, true);         //Element-wise spectrums multiplication F-1 o F-1*

    if (m_haveEps)
    {
        //Regularization parameter
        eps_curr = createEps(Bi_curr);
        Bi_curr += eps_curr;

        eps_prev = createEps(Bi_prev);
        Bi_prev += eps_curr;
    }

    // MOSSE update
    A = (((1.0 - m_learningRate) * Ai_curr) + ((m_learningRate) * Ai_prev));
    B = (((1.0 - m_learningRate) * Bi_curr) + ((m_learningRate) * Bi_prev));
    dftDiv(A, B, filter);
    filter.copyTo(this->m_filter);
}

void Tracker::updateRoi(cv::Point new_center, bool scale_rot)
{
    int diff_x, diff_y;
    //Current ROI pos is previous ROI pos
    this->m_prevRoi = this->m_currRoi;
    this->m_currRoi.ROI_center = new_center;
    new_center.x += this->m_prevRoi.ROI.x;
    new_center.y += this->m_prevRoi.ROI.y;

    //Handle image boundarie
    diff_x = new_center.x - round(this->m_currRoi.ROI.width / 2);
    diff_y = new_center.y - round(this->m_currRoi.ROI.height / 2);

    if (diff_x < 0)
    {
        this->m_currRoi.ROI.x = 0;
    }
    else if ((diff_x + this->m_currRoi.ROI.width) >= this->m_imgSize.width)
    {
        this->m_currRoi.ROI.x = this->m_imgSize.width - this->m_currRoi.ROI.width - 1;
    }
    else{
        this->m_currRoi.ROI.x = diff_x;
    }

    if (diff_y < 0)
    {
        this->m_currRoi.ROI.y = 0;
    }
    else if ((diff_y + this->m_currRoi.ROI.height) >= this->m_imgSize.height)
    {
        this->m_currRoi.ROI.y = this->m_imgSize.height - this->m_currRoi.ROI.height - 1;
    }
    else{
        this->m_currRoi.ROI.y = diff_y;
    }

    this->m_currRoi.ROI.width = this->m_prevRoi.ROI.width;
    this->m_currRoi.ROI.height = this->m_prevRoi.ROI.height;
}

cv::Mat Tracker::extractFeatures( cv::Mat &patch )
{
    cv::Mat featureMap;

//    IplImage iplPatch = patch;
//    CvLSVMFeatureMapCaskade *map;
//    getFeatureMaps( &iplPatch, CELL_SIZE, &map );
//    normalizeAndTruncate( map, 0.2f );
//    PCAFeatureMaps( map );          // each reduced feature vector has 31 components

//    featureMap = cv::Mat( cv::Size(map->numFeatures, map->sizeX*map->sizeY), CV_32F, map->map );
//    featureMap = featureMap.t();
//    if( m_initTrack )
//    {
//        featureMap = m_hanWin.mul( featureMap );
//    }
//    freeFeatureMapObject( &map );

    return featureMap;
}

cv::Mat Tracker::extractFeatures(image_track &input_image)
{
    cv::Mat featureMap;

//    IplImage ipl_patch = input_image.real_image;
//    CvLSVMFeatureMapCaskade *map;
//    getFeatureMaps(&ipl_patch, CELL_SIZE, &map);
//    normalizeAndTruncate(map, 0.2f);
//    PCAFeatureMaps(map);

//    featureMap = cv::Mat(cv::Size(map->numFeatures, map->sizeX * map->sizeY), CV_32F, map->map);
//    featureMap = featureMap.t();
//    if(m_initTrack)
//    {
//        featureMap = m_hanWin.mul(featureMap);
//    }
//    freeFeatureMapObject(&map);

    return featureMap;
}

void Tracker::createHannWindow( cv::Mat &_hann, int *size_path)
{
    cv::Mat hannX = cv::Mat::zeros( 1, size_path[1], CV_32FC1 );
    cv::Mat hannY = cv::Mat::zeros( size_path[0], 1, CV_32FC1 );

    for( int i = 0; i < hannX.cols; i++ )
    {
        hannX.at<float>(i) = 0.5 * (1 - cos(2 * PI * i / (hannX.cols - 1)));
    }

    for( int i = 0; i < hannY.rows; i++ )
    {
        hannY.at<float>(i) = 0.5 * (1 - cos(2 * PI * i / (hannY.rows - 1)));
    }

    cv::Mat hann2d = hannY * hannX;

    cv::Mat hann1d = hann2d.reshape(1, 1);
    _hann = cv::Mat(cv::Size(size_path[0] * size_path[1], size_path[2]), CV_32FC1, cv::Scalar(0));
    for( int i = 0; i < size_path[2]; i++ )
    {
        for( int j = 0; j < (size_path[0] * size_path[1]); j++ )
        {
            _hann.at<float>(i, j) = hann1d.at<float>(0, j);
        }
    }
}
