#include <iostream>
#include "vecmat/Vector.h"
#include "vecmat/Matrix.h"
#include <sstream>
#include <vector>
#include <OpenImageIO/imageio.h>

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

using namespace std;
OIIO_NAMESPACE_USING


// Struct Declaration
struct pixel {
    float r, g, b, a;
};


// Global Variable Declarations
int IMAGE_HEIGHT;
int IMAGE_WIDTH;
int NEW_IMAGE_HEIGHT;
int NEW_IMAGE_WIDTH;
Matrix3x3 TRANSFORM_MATRIX(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
char * OUTPUT_FILENAME;
pixel ** TRANSFORMED_PIXMAP;
Vector3d TRANSFORMED_ORIGIN(0.0, 0.0, 0.0);


/* Handles errors
 * input	- message is printed to stdout, if kill is true end program
 * output	- None
 * side effect - prints error message and kills program if kill flag is set
 */
void handleError (string message, bool kill) {
    cout << "ERROR: " << message << "\n";
    if (kill)
        exit(0);
}


/*
    Initializes a pixmap and sets it up for double array syntax for accessing elements
 */
void initializePixmap(pixel ** &pixmap, int image_width, int image_height) {
    pixmap = new pixel*[image_height];
    pixmap[0] = new pixel[image_width * image_height];

    for (int i = 1; i < image_height; i++)
        pixmap[i] = pixmap[i - 1] + image_width;
}


/*
    Flips image verticaly
 */
void flipImageVertical(float *&pixmap_vertical_flip, int height, int width, int channels) {
    pixel ** temp_pixmap;
    initializePixmap(temp_pixmap, IMAGE_WIDTH, IMAGE_HEIGHT);

    int i = 0;
    for (int row = height-1; row >= 0; row--)
        for (int col = 0; col < width; col++) {
            temp_pixmap[row][col].r = pixmap_vertical_flip[i++];
            temp_pixmap[row][col].g = pixmap_vertical_flip[i++];
            temp_pixmap[row][col].b = pixmap_vertical_flip[i++];
            temp_pixmap[row][col].a = pixmap_vertical_flip[i++];
        }

    i = 0;
    for (int row = 0; row < height; row++)
        for (int col = 0; col < width; col++) {
            pixmap_vertical_flip[i++] = temp_pixmap[row][col].r;
            pixmap_vertical_flip[i++] = temp_pixmap[row][col].g;
            pixmap_vertical_flip[i++] = temp_pixmap[row][col].b;
            pixmap_vertical_flip[i++] = temp_pixmap[row][col].a;
        }

    delete temp_pixmap;
}


/* Converts pixels from vector to pixel pointers
 * input		- vector of pixels, number of channels
 * output		- None
 * side effect	- saves pixel data from vector to PIXMAP
 */
pixel ** convertVectorToImage (vector<float> vector_pixels, int channels) {
    pixel ** image;
    initializePixmap(image, IMAGE_WIDTH, IMAGE_HEIGHT);

    int i = 0;
    if (channels == 3) {
        for (int row = IMAGE_HEIGHT-1; row >= 0; row--)
            for (int col = 0; col < IMAGE_WIDTH; col++) {
                image[row][col].r = vector_pixels[i++];
                image[row][col].g = vector_pixels[i++];
                image[row][col].b = vector_pixels[i++];
                image[row][col].a = 1.0;
            }
    }
    else if (channels == 4) {
        for (int row = IMAGE_HEIGHT-1; row >= 0; row--)
            for (int col = 0; col < IMAGE_HEIGHT; col++) {
                image[row][col].r = vector_pixels[i++];
                image[row][col].g = vector_pixels[i++];
                image[row][col].b = vector_pixels[i++];
                image[row][col].a = vector_pixels[i++];
            }
    }
    else
        handleError ("Could not convert image.. sorry", 1);

    return image;
}


/* Reads image specified in argv[1]
 * input		- the input file name
 * output		- None
 */
pixel ** readImage (string filename) {
    ImageInput *in = ImageInput::open(filename);
    if (!in)
        handleError("Could not open input file", true);
    const ImageSpec &spec = in->spec();
    IMAGE_WIDTH = spec.width;
    IMAGE_HEIGHT = spec.height;
    int channels = spec.nchannels;
    if(channels < 3 || channels > 4)
        handleError("Application supports 3 or 4 channel images only", 1);
    vector<float> pixels (IMAGE_WIDTH*IMAGE_HEIGHT*channels);
    in->read_image (TypeDesc::FLOAT, &pixels[0]);
    in->close ();
    delete in;


    return convertVectorToImage(pixels, channels);
}


/* Write image to specified file
 * input		- pixel array, width of display window, height of display window
 * output		- None
 * side effect	- writes image to a file
 */
void writeImage (pixel ** &pixmap, char * output_file_name, int window_width, int window_height) {
    const char *filename = output_file_name;
    const int xres = window_width, yres = window_height;
    const int channels = 4; // RGBA
    int scanlinesize = xres * channels;
    ImageOutput *out = ImageOutput::create (filename);
    if (! out) {
        handleError("Could not create output file", false);
        return;
    }
    ImageSpec spec (xres, yres, channels, TypeDesc::FLOAT);
    out->open (filename, spec);
    out->write_image (TypeDesc::FLOAT, pixmap);
    out->close ();
    delete out;
    cout << "SUCCESS: Image successfully written to " << output_file_name << "\n";
}


/*
    adjusts tranform matrix for a rotation
 */
void calculateRotationTransform (double theta) {
    int row, col;
    Matrix3x3 rotation_matrix(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
    double angle_in_radians, cos_of_angle, sin_of_angle;

    angle_in_radians = PI * theta / 180.0;
    cos_of_angle = cos(angle_in_radians);
    sin_of_angle = sin(angle_in_radians);

    rotation_matrix[0][0] = cos_of_angle;
    rotation_matrix[0][1] = -sin_of_angle;
    rotation_matrix[1][0] = sin_of_angle;
    rotation_matrix[1][1] = cos_of_angle;

    Matrix3x3 product_matrix = rotation_matrix * TRANSFORM_MATRIX;

    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
}


/*
    adjusts tranform matrix for a scale
 */
void calculateScaleTransform(double x_scale, double y_scale) {
    int row, col;
    Matrix3x3 scale_matrix(x_scale, 0.0, 0.0, 0.0, y_scale, 0.0, 0.0, 0.0, 1.0);

    Matrix3x3 product_matrix = scale_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
}


/*
    adjusts tranform matrix for a translate
 */
void calculateTranslateTransform(double x_translate, double y_translate) {
    int row, col;
    Matrix3x3 translate_matrix(1.0, 0.0, x_translate, 0.0, 1.0, y_translate, 0.0, 0.0, 1.0);

    Matrix3x3 product_matrix = translate_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
}


/*
    adjusts tranform matrix for a flip
 */
void calculateFlipTransform(int x_flip, int y_flip) {
    int row, col;
    Matrix3x3 flip_matrix(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);

    if (x_flip == 1) flip_matrix[0][0] = -1.0;
    if (y_flip == 1) flip_matrix[1][1] = -1.0;

    Matrix3x3 product_matrix = flip_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
}


/*
    adjusts tranform matrix for a shear
 */
void calculateShearTransform(double x_shear, double y_shear) {
    int row, col;
    Matrix3x3 shear_matrix(1.0, x_shear, 0.0, y_shear, 1.0, 0.0, 0.0, 0.0, 1.0);

    Matrix3x3 product_matrix = shear_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
}


/*
    adjusts tranform matrix for a perspective
 */
void calculatePerspectiveTransform(double x_perspective, double y_perspective) {
    int row, col;
    Matrix3x3 perspective_matrix(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, x_perspective, y_perspective, 1.0);


    Matrix3x3 product_matrix = perspective_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
}


/*
    Control logic for different input strings
 */
void calculateTransformMatrix(string user_input) {
    double theta;
    double x_scale, y_scale;
    double x_translate, y_translate;
    int x_flip, y_flip;
    double x_shear, y_shear;
    double x_perspective, y_perspective;
    string temp;
    stringstream string_stream(user_input);

    if (user_input.compare(0, 1, "r") == 0) {
        string_stream >> temp >> theta;

        cout << "\nRotation applied. ENTER next command (ENTER d when finished):\n";
        calculateRotationTransform(theta);
    }
    else if (user_input.compare(0, 1, "s") == 0) {
        string_stream >> temp >> x_scale >> y_scale;

        cout << "\nScale applied. ENTER next command (ENTER d when finished):\n";
        calculateScaleTransform(x_scale, y_scale);
    }
    else if (user_input.compare(0, 1, "t") == 0) {
        string_stream >> temp >> x_translate >> y_translate;

        cout << "\nTranslate applied. ENTER next command (ENTER d when finished):\n";
        calculateTranslateTransform(x_translate, y_translate);
    }
    else if (user_input.compare(0, 1, "f") == 0) {
        string_stream >> temp >> x_flip >> y_flip;

        cout << "\nFlip applied. ENTER next command (ENTER d when finished):\n";
        calculateFlipTransform(x_flip, y_flip);
    }
    else if (user_input.compare(0, 1, "h") == 0) {
        string_stream >> temp >> x_shear >> y_shear;

        cout << "\nShear applied. ENTER next command (ENTER d when finished):\n";
        calculateShearTransform(x_shear, y_shear);
    }
    else if (user_input.compare(0, 1, "p") == 0) {
        string_stream >> temp >> x_perspective >> y_perspective;

        cout << "\nPerspective applied. ENTER next command (ENTER d when finished):\n";
        calculatePerspectiveTransform(x_perspective, y_perspective);
    }
}


/*
    Gets the dimensions of the new image by performing a forward map on the four corners of the original image.
    The max and min height and width values are used to get the dimensions of the new image.
 */
void getNewImageDimensions () {
    int transformed_max_width, transformed_max_height;
    int transformed_min_width, transformed_min_height;

    Vector3d bottom_left_corner(0, 0, 1.0);
    Vector3d bottom_right_corner(IMAGE_WIDTH - 1.0, 0.0, 1.0);
    Vector3d top_left_corner(0, IMAGE_HEIGHT - 1, 1.0);
    Vector3d top_right_corner(IMAGE_WIDTH, IMAGE_HEIGHT, 1.0);

    cout << "\nCorners of transformed image\n";
    cout << bottom_left_corner << " " << top_left_corner << " " << top_right_corner << " " << bottom_right_corner;

    // get transformed corners
    Vector3d transformed_bottom_left_corner = TRANSFORM_MATRIX * bottom_left_corner;
    Vector3d transformed_bottom_right_corner = TRANSFORM_MATRIX * bottom_right_corner;
    Vector3d transformed_top_left_corner = TRANSFORM_MATRIX * top_left_corner;
    Vector3d transformed_top_right_corner = TRANSFORM_MATRIX * top_right_corner;

    cout << "\nCorners of transformed image\n";
    cout << transformed_bottom_left_corner << " " << transformed_top_left_corner << " " << transformed_top_right_corner << " " << transformed_bottom_right_corner;

    // normalize transformed corners
    transformed_bottom_left_corner = transformed_bottom_left_corner / transformed_bottom_left_corner[2];
    transformed_bottom_right_corner = transformed_bottom_right_corner / transformed_bottom_right_corner[2];
    transformed_top_left_corner = transformed_top_left_corner / transformed_top_left_corner[2];
    transformed_top_right_corner = transformed_top_right_corner / transformed_top_right_corner[2];

    cout << "\nCorners of transformed image after normalization\n";
    cout << transformed_bottom_left_corner << " " << transformed_top_left_corner << " " << transformed_top_right_corner << " " << transformed_bottom_right_corner;

    // get max and min width and height
    transformed_max_width = (int) max(max(transformed_bottom_left_corner[0], transformed_bottom_right_corner[0]),
                                     max(transformed_top_left_corner[0], transformed_top_right_corner[0]));

    transformed_max_height = (int) max(max(transformed_bottom_left_corner[1], transformed_bottom_right_corner[1]),
                                     max(transformed_top_left_corner[1], transformed_top_right_corner[1]));

    transformed_min_width = (int) min(min(transformed_bottom_left_corner[0], transformed_bottom_right_corner[0]),
                                       min(transformed_top_left_corner[0], transformed_top_right_corner[0]));

    transformed_min_height = (int) min(min(transformed_bottom_left_corner[1], transformed_bottom_right_corner[1]),
                                       min(transformed_top_left_corner[1], transformed_top_right_corner[1]));
    cout << "\ntransformed max and min height and width\n";
    cout << transformed_max_width << " " << transformed_max_height << " " << transformed_min_width << " " << transformed_min_height << "\n";

    // set global transformed origin
    TRANSFORMED_ORIGIN[0] = transformed_min_width;
    TRANSFORMED_ORIGIN[1] = transformed_min_height;

    // calculate new image width and height
    NEW_IMAGE_WIDTH = abs(transformed_max_width - transformed_min_width);
    NEW_IMAGE_HEIGHT = abs(transformed_max_height - transformed_min_height);
}


/*
    Populates new image by performing an inverse map on the original image
 */
void populateTransformedPixmap(pixel ** &pixmap) {

    Matrix3x3 inverse_matrix = TRANSFORM_MATRIX.inv();

    cout << "\nCalculated Inverse Matrix:\n";
    cout << inverse_matrix;

     for (int row = 0; row < NEW_IMAGE_HEIGHT; row++)
        for (int col = 0; col < NEW_IMAGE_WIDTH; col++) {
            Vector3d pixel_out(col, row, 1.0);
            pixel_out = pixel_out + TRANSFORMED_ORIGIN;
            Vector3d pixel_in = inverse_matrix * pixel_out;

            // normalize the pixmap
            float u = (float) pixel_in[0] / pixel_in[2];
            float v = (float) pixel_in[1] / pixel_in[2];


            // basic interpolation
            if ((int) round(v) > (IMAGE_HEIGHT - 1) or (int) round(u) > (IMAGE_WIDTH - 1) or
                    (int) round(v) < 0 or (int) round(u) < 0) {
                TRANSFORMED_PIXMAP[row][col].r = 0.0;
                TRANSFORMED_PIXMAP[row][col].g = 0.0;
                TRANSFORMED_PIXMAP[row][col].b = 0.0;
                TRANSFORMED_PIXMAP[row][col].a = 0.0;
            }
            else
                TRANSFORMED_PIXMAP[row][col] = pixmap[(int)round(v)][(int)round(u)];
        }
}


/* Draw Image to opengl display
 * input		- None
 * output		- None
 * side effect	- draws image to opengl display window
 */
void drawImage() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glRasterPos2i(0,0);
    glDrawPixels(NEW_IMAGE_WIDTH, NEW_IMAGE_HEIGHT, GL_RGBA, GL_FLOAT, TRANSFORMED_PIXMAP[0]);
    glFlush();
}


/* Key press handler
 * input	- Handled by opengl, because this is a callback function.
 * output	- None
 */
void handleKey(unsigned char key, int x, int y) {
    if (key == 'q' || key == 'Q') {
        cout << "\nProgram Terminated." << endl;
        exit(0);
    }
}


/* Initialize opengl
 * input	- command line arguments
 * output	- none
 */
void openGlInit(int argc, char* argv[]) {
    // start up the glut utilities
    glutInit(&argc, argv);

    // create the graphics window, giving width, height, and title text
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
    glutInitWindowSize(NEW_IMAGE_WIDTH, NEW_IMAGE_HEIGHT);
    glutCreateWindow("Warp Result");

    // set up the callback routines to be called when glutMainLoop() detects
    // an event
    glutDisplayFunc(drawImage);		  		// display callback
    glutKeyboardFunc(handleKey);	  		// keyboard callback

    // define the drawing coordinate system on the viewport
    // lower left is (0, 0), upper right is (WIDTH, HEIGHT)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, NEW_IMAGE_WIDTH, 0, NEW_IMAGE_HEIGHT);

    // specify window clear (background) color to be opaque white
    glClearColor(1, 1, 1, 0);

    // Routine that loops forever looking for events. It calls the registered
    // callback routine to handle each event that is detected
    glutMainLoop();
}


int main(int argc, char *argv[]) {
    char *output_file_name;
    pixel ** pixmap;
    string user_input = "null"; // initialize string to a word that does not start with the letter 'd'

    // check for valid argument values
    if (argc != 2 and argc != 3)
        handleError("Proper use:\n$> warper input.img [output.img]", 1);

    if (argc == 3)
        OUTPUT_FILENAME = argv[2];

    // read the input image
    pixmap = readImage(argv[1]);

    // Get user input and build transformation matrix
    while(user_input.compare(0, 1, "d") != 0) {
        getline(cin,user_input);
        calculateTransformMatrix(user_input);
        cout << TRANSFORM_MATRIX << "\n";

    }

    cout << "\nDone transforming matrix\nFinal transform matrix is:\n";
    cout << TRANSFORM_MATRIX;

    // create a new image based on the forward transform of the corners of the input image
    getNewImageDimensions();
    initializePixmap(TRANSFORMED_PIXMAP, NEW_IMAGE_WIDTH, NEW_IMAGE_HEIGHT);

    // old width and height
    cout << "\nOld Image Width and Height\n";
    cout << IMAGE_WIDTH << " " << IMAGE_HEIGHT << " " << endl;

    // new width and height
    cout << "\nNew Image Width and Height.\n";
    cout << NEW_IMAGE_WIDTH << " " << NEW_IMAGE_HEIGHT << endl;

    populateTransformedPixmap(pixmap);

    if (argc == 3) // specified output file
        writeImage(TRANSFORMED_PIXMAP, argv[2], NEW_IMAGE_WIDTH, NEW_IMAGE_HEIGHT);

    openGlInit(argc, argv);
    return 0;
}