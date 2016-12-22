#include <string>
#include <map>
#include <array>

#include <SFML/Graphics/Shader.hpp>

#include "shaders.hpp"

#define S(...) #__VA_ARGS__

static std::string scalemap = S(
    uniform float scale;
    uniform float bias;

    float scalemap(float p) {
        return clamp(p * scale + bias, 0.0, 1.0);
    }
    vec2 scalemap(vec2 p) {
        return clamp(p * scale + bias, 0.0, 1.0);
    }
    vec3 scalemap(vec3 p) {
        return clamp(p * scale + bias, 0.0, 1.0);
    }
    vec4 scalemap(vec4 p) {
        return clamp(p * scale + bias, 0.0, 1.0);
    }
);

static std::string hsvtorgb_glsl = S(
    vec3 hsvtorgb(vec3 colo)
    {
        vec4 outp;
        float r, g, b, h, s, v;
        r=g=b=h=s=v=0.0;
        h = colo.x; s = colo.y; v = colo.z;
        if (s == 0.0) { r = g = b = v; }
        else {
            float H = mod(floor(h/60.0) , 6.0);
            float p, q, t, f = h/60.0 - H;
            p = v * (1.0 - s);
            q = v * (1.0 - f*s);
            t = v * (1.0 - (1.0 - f)*s);
            if(H == 6.0 || H == 0.0) { r = v; g = t; b = p; }
            else if(H == -1.0 || H == 5.0) { r = v; g = p; b = q; } 
            else if(H == 1.0) { r = q; g = v; b = p; }
            else if(H == 2.0) { r = p; g = v; b = t; }
            else if(H == 3.0) { r = p; g = q; b = v; }
            else if(H == 4.0) { r = t; g = p; b = v; }
        }
        return vec3(r, g, b);
    }
);

static std::string atan2_glsl = S(
    float M_PI = 3.1415926535897932;
    float M_PI_2 = 1.5707963267948966;
    float atan2(float x, float y)
    {
       if (x>0.0) { return atan(y/x); }
       else if(x<0.0 && y>0.0) { return atan(y/x) + M_PI; }
       else if(x<0.0 && y<=0.0 ) { return atan(y/x) - M_PI; }
       else if(x==0.0 && y>0.0 ) { return M_PI_2; }
       else if(x==0.0 && y<0.0 ) { return -M_PI_2; }
       return 0.0;
    }
);

static std::string defaultVertex = S(
    void main()
    {
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
        gl_FrontColor = gl_Color;
    }
);

static std::string mainFragment = scalemap + S(
    uniform sampler2D texture;

    void main()
    {
        vec4 border = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 p = texture2D(texture, gl_TexCoord[0].xy);
        if (gl_TexCoord[0].x < 0.0 || gl_TexCoord[0].x > 1.0) {
            gl_FragColor = border;
        } else if (gl_TexCoord[0].y < 0.0 || gl_TexCoord[0].y > 1.0) {
            gl_FragColor = border;
        } else {
            gl_FragColor = vec4(tonemap(scalemap(p)), 1.0);
        }
    }
);

static std::string rgbTonemap = S(
    vec3 tonemap(vec4 p)
    {
        return p.xyz;
    }
);

static std::string grayTonemap = S(
    vec3 tonemap(vec4 p)
    {
        return vec3(p.x);
    }
);

// from https://github.com/gfacciol/pvflip
static std::string opticalFlowTonemap = hsvtorgb_glsl + atan2_glsl + S(
    vec3 tonemap(vec4 p)
    {
        float a = (180.0/M_PI)*(atan2(-p.x, p.y) + M_PI);
        float r = sqrt(p.x*p.x+p.w*p.w);
        vec3 q = vec3(a, r, r);
        return hsvtorgb(q);
    }
);

// from https://github.com/gfacciol/pvflip
static std::string jetTonemap =  S(
    vec3 tonemap(vec4 q)
    {
        float d = q.x;
        if(d < 0.0) d = -0.05;
        if(d > 1.0) d =  1.05;
        d = d/1.15 + 0.1;
        vec3 p;
        p.x = 1.5 - abs(d - .75)*4.0;
        p.y = 1.5 - abs(d - .50)*4.0;
        p.z = 1.5 - abs(d - .25)*4.0;
        return p;
    }
);

std::map<std::string, sf::Shader> gShaders;

void loadShaders()
{
    gShaders["default"].loadFromMemory(defaultVertex, rgbTonemap + mainFragment);
    gShaders["gray"].loadFromMemory(defaultVertex, grayTonemap + mainFragment);
    gShaders["opticalFlow"].loadFromMemory(defaultVertex, opticalFlowTonemap + mainFragment);
    gShaders["jet"].loadFromMemory(defaultVertex, jetTonemap + mainFragment);
}

