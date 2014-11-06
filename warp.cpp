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
Matrix3x3 TRANSFORM_MATRIX(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
char * OUTPUT_FILENAME;



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
void initializePixmap(pixel ** &pixmap) {
    pixmap = new pixel*[IMAGE_HEIGHT];
    pixmap[0] = new pixel[IMAGE_WIDTH * IMAGE_HEIGHT];

    for (int i = 1; i < IMAGE_HEIGHT; i++)
        pixmap[i] = pixmap[i - 1] + IMAGE_WIDTH;
}


/*
    Flips image verticaly
 */
void flipImageVertical(float *&pixmap_vertical_flip, int height, int width, int channels) {
    pixel ** temp_pixmap;
    initializePixmap(temp_pixmap);

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
    initializePixmap(image);

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

    cout << TRANSFORM_MATRIX;
}


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


void calculateFlipTransform(int x_flip, int y_flip) {
    int row, col;
    Matrix3x3 flip_matrix(-1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 1.0);

    Matrix3x3 product_matrix = flip_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }

    // figure it out
}


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


void calculatePerspectiveTransform(double x_perspective, double y_perspective) {
    int row, col;
    Matrix3x3 perspective_matrix(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, x_perspective, y_perspective, 1.0);


    Matrix3x3 product_matrix = perspective_matrix * TRANSFORM_MATRIX;
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 3; col++) {
            TRANSFORM_MATRIX[row][col] = product_matrix[row][col];
        }
    }
    // a31 a32
}


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

pixel ** createNewImage (pixel ** pixmap) {
    pixel ** transformed_pixmap;
    initializePixmap(transformed_pixmap);


}

int main(int argc, char *argv[]) {
    char *output_file_name;
    pixel ** pixmap;
    pixel ** transformed_pixmap;
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
    createNewImage(pixmap);

    if (argc == 3) // specified output file
        writeImage(pixmap, argv[2], IMAGE_WIDTH, IMAGE_HEIGHT);

    return 0;
}