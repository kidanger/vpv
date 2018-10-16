#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "imgui_custom.hpp"

#include "Image.hpp"
#include "globals.hpp"
#include "Histogram.hpp"

namespace imscript {
    // a quad is a square cell bounded by 4 pixels
    // it knows its four values and its position on the original image
    struct quad {
        float abcd[4];
        int ij;
    };

    static int compare_floats(const void *a, const void *b)
    {
        const float *da = (const float *) a;
        const float *db = (const float *) b;
        return (*da > *db) - (*da < *db);
    }

    static void sort_four_values(float *x)
    {
        qsort(x, 4, sizeof*x, compare_floats);
    }

    // copy the values of a square cell into an array, for easy access
    static void copy_cell_values(float q[4], float *x, int w, int h, int c, int i, int j)
    {
        assert(0 <= i); assert(i < w - 1);
        assert(0 <= j); assert(j < h - 1);
        q[0] = x[ ((j+0)*w + (i+0))*c ];
        q[1] = x[ ((j+0)*w + (i+1))*c ];
        q[2] = x[ ((j+1)*w + (i+0))*c ];
        q[3] = x[ ((j+1)*w + (i+1))*c ];
    }

    // obtain the histogram bin that corresponds to the given value
    static int bin(int n, float m, float M, float x)
    {
        float f = (n - 1) * (x - m) / (M - m);
        int r = lrint(f);
        assert(m <= x); assert(x <= M); assert(0 <= r); assert(r < n);
        return r;
    }

    static int cell_is_degenerate(float m, float M, float q[4])
    {
        for (int i = 0; i < 4; i++)
            if (q[i] < m || q[i] > M)
                return 1;
        return !(q[0] < q[1] && q[1] < q[2] && q[2] < q[3]);
    }

    static void integrate_values(long double (*o)[2], int n)
    {
        // TODO : multiply each increment by the span of the interval
        for (int i = 1; i < n; i++)
            o[i][1] += o[i-1][1];// * (o[i][0] - o[i-1][0]);
        //o[0][1] = 0;
    }

    static void accumulate_jumps_for_one_cell(long double (*o)[2],
                                              int n, float m, float M, float q[4])
    {
        // discard degenerate cells
        if (cell_is_degenerate(m, M, q))
            return;

        // give nice names to numbers
        float A = q[0];
        float B = q[1];
        float C = q[2];
        float D = q[3];
        int i_A = bin(n, m, M, A);
        int i_B = bin(n, m, M, B);
        int i_C = bin(n, m, M, C);
        int i_D = bin(n, m, M, D);
        A = i_A;
        B = i_B;
        C = i_C;
        D = i_D;

        if (i_A == i_B || i_B == i_C || i_C == i_D) return;

        assert(i_A < i_B);
        assert(i_B < i_C);
        assert(i_C < i_D);

        double fac = 1;//33333;

        // accumulate jumps
        o[ i_A ][1] += fac * 2 / (C + D - B - A) / (B - A);
        o[ i_B ][1] -= fac * 2 / (C + D - B - A) / (B - A);
        o[ i_C ][1] -= fac * 2 / (C + D - B - A) / (D - C);
        o[ i_D ][1] += fac * 2 / (C + D - B - A) / (D - C);
    }

    static void fill_continuous_histogram_simple(
                                          long double (*o)[2], // output histogram array of (value,density) pairs
                                          int n,               // requested number of bins for the histogram
                                          float m,             // requested minimum of the histogram
                                          float M,             // requested maximum of the histogram
                                          float *x,            // input image data
                                          int w,               // input image width
                                          int h,                // input image height
                                          int c
                                         )
    {
        // initialize bins (n equidistant points between m and M)
        for (int i = 0; i < n; i++) {
            o[i][0] = m + i * (M - m) / n;
            o[i][1] = 0;
        }

        // compute 2nd derivative of histogram
        for (int j = 0; j < h - 1; j++)
            for (int i = 0; i < w - 1; i++) {
                float q[4]; // here we store the 4 values of the cell at (i,j)
                copy_cell_values(q, x, w, h, c, i, j);
                sort_four_values(q);
                accumulate_jumps_for_one_cell(o, n, m, M, q);
            }

        // integrate twice
        integrate_values(o, n);
        integrate_values(o, n);
    }
}

void Histogram::request(std::shared_ptr<Image> image, float min, float max, Mode mode) {
    std::lock_guard<std::recursive_mutex> _lock(lock);
    std::shared_ptr<Image> img = this->image.lock();
    if (image == img && min == this->min && max == this->max && mode == this->mode)
        return;
    loaded = false;
    this->mode = mode;
    this->min = min;
    this->max = max;
    this->image = image;
    curh = 0;

    values.clear();
    values.resize(image->c);

    for (size_t d = 0; d < image->c; d++) {
        auto& histogram = values[d];
        histogram.clear();
        histogram.resize(nbins);
    }
}

float Histogram::getProgressPercentage() const {
    std::shared_ptr<Image> image = this->image.lock();
    if (loaded) return 1.f;
    if (!image) return 0.f;
    return (float) curh / image->h;
}

void Histogram::progress()
{
    std::vector<std::vector<long>> valuescopy;
    size_t oldh;
    {
        std::lock_guard<std::recursive_mutex> _lock(lock);
        valuescopy = values;
        oldh = curh;
    }

    std::shared_ptr<Image> image = this->image.lock();
    if (!image) return;

    if (mode == EXACT) {
        for (size_t d = 0; d < image->c; d++) {
            auto& histogram = valuescopy[d];
            // nbins-1 because we want the last bin to end at 'max' and not start at 'max'
            float f = (nbins-1) / (max - min);
            for (size_t i = 0; i < image->w; i++) {
                int bin = (image->pixels[(curh*image->w + i)*image->c+d] - min) * f;
                if (bin >= 0 && bin < nbins) {
                    histogram[bin]++;
                }
            }
        }
    } else if (mode == SMOOTH) {
        long double bins[3+nbins][2];
        for (size_t d = 0; d < image->c; d++) {
            imscript::fill_continuous_histogram_simple(bins, nbins, min, max, image->pixels+d, image->w, image->h, image->c);
            for (int b = 0; b < nbins; b++) {
                valuescopy[d][b] = bins[b][1];
            }
        }
    }

    {
        std::lock_guard<std::recursive_mutex> _lock(lock);
        if (oldh != curh) {
            // someone called request()
            return;
        }
        if (mode == EXACT) {
            curh++;
        } else {
            curh = image->h;
        }

        if (curh == image->h) {
            loaded = true;
        }

        values = valuescopy;
    }
}

void Histogram::draw(const std::array<float,3>& highlightmin, const std::array<float,3>& highlightmax)
{
    std::lock_guard<std::recursive_mutex> _lock(lock);
    const size_t c = values.size();

    const void* vals[4];
    for (size_t d = 0; d < c; d++) {
        vals[d] = this->values[d].data();
    }

    const char* names[] = {"r", "g", "b", ""};
    ImColor colors[] = {
        ImColor(255, 0, 0), ImColor(0, 255, 0), ImColor(0, 0, 255), ImColor(100, 100, 100)
    };
    if (c == 1) {
        colors[0] = ImColor(255, 255, 255);
        names[0] = "";
    }
    auto getter = [](const void *data, int idx) {
        const long* hist = (const long*) data;
        return (float)hist[idx];
    };

    int boundsmin[c];
    int boundsmax[c];
    float f = (nbins-1) / (max - min);
    for (size_t d = 0; d < c; d++) {
        if (d < 3) {
            boundsmin[d] = std::floor((highlightmin[d] - min) * f);
            boundsmax[d] = std::ceil((highlightmax[d] - min) * f);
        }
    }

    ImGui::Separator();
    ImGui::PlotMultiHistograms("", c, names, colors, getter, vals,
                               nbins, FLT_MIN, curh?FLT_MAX:1.f, ImVec2(nbins, 80),
                               boundsmin, boundsmax);
    if (ImGui::BeginPopupContextItem("")) {
        bool smooth = gSmoothHistogram;
        if (ImGui::Checkbox("Smooth", &smooth)) {
            gSmoothHistogram = smooth;
            request(image.lock(), min, max, gSmoothHistogram ? SMOOTH : EXACT);
        }
        ImGui::EndPopup();
    }

    if (!isLoaded()) {
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        const ImU32 bg = ImColor(100,100,100);
        ImGui::BufferingBar("##bar", getProgressPercentage(),
                            ImVec2(ImGui::GetWindowWidth()-10, 6), bg, col);
    }
}

