#include <iostream>
#include "CImg.h" // Documentation: https://www.cimg.eu/reference/
using namespace cimg_library;

const int TILE_ROW_NUM = 2;
const int TILE_COL_NUM = 3;
 
int main() {
  std::string image_name;
  std::cout << "Which image?\n";
  std::cin >> image_name;
  const char* image_input = image_name.c_str();
  CImg<unsigned char> image(image_input); // replace with image to test

  image.blur(7.2); // blur image to "erase" small details

  // 11.10 - 3 by 2 grid
  // for now, grid will be split evenly (i.e. into half for rows, into thirds for columns)
  float tile_lum[TILE_ROW_NUM][TILE_COL_NUM]; // stores the average luminance per tile
  int tile_size = (image.height() / TILE_ROW_NUM) * (image.width() / TILE_COL_NUM);

  for (int tile_row = 0; tile_row < TILE_ROW_NUM; ++tile_row) { // iterate through rows of tiles
    int start_row = tile_row * (image.height() / TILE_ROW_NUM);
    int end_row = (tile_row + 1) * (image.height() / TILE_ROW_NUM);
    for (int tile_col = 0; tile_col < TILE_COL_NUM; ++tile_col) { // iterate through columns of tiles
      int start_col = tile_col * (image.width() / TILE_COL_NUM);
      int end_col = (tile_col + 1) * (image.width() / TILE_COL_NUM);
      float total_lum = 0; // stores the total luminance of all the pixels within tile
      for (int y = start_row; y < end_row; ++y) { // iterate through rows within tile
        for (int x = start_col; x < end_col; ++x) { // iterate through columns within tiles
          // calculate luminance i.e. lightness in pixel; formula here: https://en.wikipedia.org/wiki/HSL_and_HSV
          float maxPx = (float)std::max((int)image(x, y, 0, 0), std::max((int)image(x, y, 0, 1), (int)image(x, y, 0, 2))) / 255.0; // max of RGB values
          float minPx = (float)std::min((int)image(x, y, 0, 0), std::min((int)image(x, y, 0, 1), (int)image(x, y, 0, 2))) / 255.0; // min of RGB values
          total_lum += (maxPx + minPx) / 2;
        }
      }
      tile_lum[tile_row][tile_col] = total_lum / (float)tile_size; // set average luminance in tiles
    }
  }

  // print out un-normalized tile values
  std::cout << "Before normalization:\n";
  for (int i = 0; i < TILE_ROW_NUM; ++i) {
    for (int j = 0; j < TILE_COL_NUM; ++j) {
      std::cout << tile_lum[i][j] << " ";
    }
    std::cout << std::endl;
  }

  // retrieve min and max lum from tiles
  float min_lum = tile_lum[0][0];
  float max_lum = tile_lum[0][0];
  for (int i = 0; i < TILE_ROW_NUM; ++i) {
    for (int j = 0; j < TILE_COL_NUM; ++j) {
      if (tile_lum[i][j] < min_lum) min_lum = tile_lum[i][j];
      if (tile_lum[i][j] > max_lum) max_lum = tile_lum[i][j];
    }
  }

  // normalize tile lum values for easier comparison
  for (int i = 0; i < TILE_ROW_NUM; ++i) {
    for (int j = 0; j < TILE_COL_NUM; ++j) {
      tile_lum[i][j] = (tile_lum[i][j] - min_lum) / (max_lum - min_lum);
    }
  }

  // print out normalized tile values
  std::cout << "After normalization:\n";
  for (int i = 0; i < TILE_ROW_NUM; ++i) {
    for (int j = 0; j < TILE_COL_NUM; ++j) {
      std::cout << tile_lum[i][j] << " ";
    }
    std::cout << std::endl;
  }

  // Check for end of line condition: Top row lum is all >= 0.5 i.e. more light than dark
  bool top_white = true;
  for (int j = 0; j < TILE_COL_NUM; ++j) {
    if (tile_lum[0][j] < 0.5) {
      top_white = false;
      break;
    }
  }
  if (top_white) {
    std::cout << "Reached end\n";
    return 0;
  }

  // Check for direction: lum of bottom row
  bool left_white = tile_lum[TILE_ROW_NUM - 1][0] >= 0.5;
  bool right_white = tile_lum[TILE_ROW_NUM - 1][TILE_COL_NUM - 1] >= 0.5;
  if (left_white == right_white || abs(tile_lum[TILE_ROW_NUM - 1][0] - tile_lum[TILE_ROW_NUM - 1][TILE_COL_NUM - 1]) < 0.1) std::cout << "Go straight\n"; // extra check if lum of left and right are similar
  else if (left_white) std::cout << "Turn right\n";
  else if (right_white) std::cout << "Turn left\n";

  return 0;
}

/* 11.08 - Converting to BW
float totalLum = 0;
cimg_forXY(image,x,y) {
  float maxPx = (float)std::max((int)image(x, y, 0, 0), std::max((int)image(x, y, 0, 1), (int)image(x, y, 0, 2))) / 255.0;
  float minPx = (float)std::min((int)image(x, y, 0, 0), std::min((int)image(x, y, 0, 1), (int)image(x, y, 0, 2))) / 255.0;
  totalLum += (maxPx + minPx) / 2;
}
float lumThreshold = totalLum / (image.height() * image.width());
lumThreshold *= 0.8; // correction factor
std::cout << "threshold: " << lumThreshold << std::endl;
cimg_forXY(image,x,y) {
  float lum = 0;
  float maxPx = (float)std::max((int)image(x, y, 0, 0), std::max((int)image(x, y, 0, 1), (int)image(x, y, 0, 2))) / 255.0;
  float minPx = (float)std::min((int)image(x, y, 0, 0), std::min((int)image(x, y, 0, 1), (int)image(x, y, 0, 2))) / 255.0;
  lum = (maxPx + minPx) / 2;
  if (lum > lumThreshold) { // luminance i.e. brightness above threshold
    image(x, y, 0, 0) = image(x, y, 0, 1) = image(x, y, 0, 2) = 255; // set to white
  } else {
    image(x, y, 0, 0) = image(x, y, 0, 1) = image(x, y, 0, 2) = 0; // set to black
  }
}
image.save_jpeg("output.jpg");

// determine direction
bool top_all_white = true; // check if top is all white i.e. past end of line
cimg_forX(image, x) {
  if ((int)image(x, 0, 0, 0) != 255) {
    top_all_white = false;
    break;
  }
}
if (top_all_white) std::cout << "Reached end\n";
else {
  float left_col = 0, right_col = 0;
  cimg_forY(image, y) {
    left_col += (int)image(0, y, 0, 0);
    right_col += (int)image(image.width() - 1, y, 0, 0);
  }
  left_col /= (image.height() * 255);
  right_col /= (image.height() * 255);

  bool left_col_white = (left_col > 0.5);
  bool right_col_white = (right_col > 0.5);

  if (left_col_white == right_col_white) std::cout << "Go straight\n";
  else if (left_col_white) std::cout << "Go right\n";
  else std::cout << "Go left\n";
}
*/