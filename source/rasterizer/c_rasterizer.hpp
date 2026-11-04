#pragma once

#include <SDL3/SDL_opengl.h>

#include <array>
#include <cstddef>
#include <cstdint>

class c_rasterizer
{
public:
    using boolean = GLboolean;
    using byte = GLbyte;
    using ubyte = GLubyte;
    using short_t = GLshort;
    using ushort_t = GLushort;
    using int_t = GLint;
    using uint_t = GLuint;
    using float_t = GLfloat;
    using double_t = GLdouble;
    using clampf = GLclampf;
    using bitfield = GLbitfield;
    using sizei = GLsizei;

    // State / misc
    static void enable(GLenum cap);
    static void disable(GLenum cap);
    static void depth_mask(GLboolean flag);
    static void clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
    static void clear(GLbitfield mask);
    static void viewport(GLint x, GLint y, GLsizei width, GLsizei height);
    static void matrix_mode(GLenum mode);
    static void load_identity();
    static void ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
                      GLdouble z_near, GLdouble z_far);
    static void frustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
                        GLdouble z_near, GLdouble z_far);
    static void rotate(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
    static void translate(GLfloat x, GLfloat y, GLfloat z);
    static void alpha_func(GLenum func, GLclampf ref);
    static void blend_func(GLenum sfactor, GLenum dfactor);
    static void color3f(GLfloat r, GLfloat g, GLfloat b);
    static void color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
    static void begin(GLenum mode);
    static void end();
    static void vertex3f(GLfloat x, GLfloat y, GLfloat z);
    static void vertex2f(GLfloat x, GLfloat y);
    static void normal3f(GLfloat x, GLfloat y, GLfloat z);
    static void tex_coord2f(GLfloat s, GLfloat t);
    static void push_matrix();
    static void pop_matrix();
    static void mult_matrix(const GLfloat* m);

    // Textures
    static void gen_textures(GLsizei n, GLuint* textures);
    static void bind_texture(GLenum target, GLuint texture);
    static void tex_parameteri(GLenum target, GLenum pname, GLint param);
    static void tex_image_2d(GLenum target, GLint level, GLint internal_format,
                             GLsizei width, GLsizei height, GLint border,
                             GLenum format, GLenum type, const GLvoid* pixels);
    static void tex_envi(GLenum target, GLenum pname, GLint param);

    // Display lists
    static uint_t gen_lists(GLsizei range);
    static void new_list(GLuint list, GLenum mode);
    static void end_list();
    static void call_list(GLuint list);

    // Lighting
    static void light_modeli(GLenum pname, GLint param);
    static void light_modelfv(GLenum pname, const GLfloat* params);
    static void light_fv(GLenum light, GLenum pname, const GLfloat* params);
    static void color_material(GLenum face, GLenum mode);
};