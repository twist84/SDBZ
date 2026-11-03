#include "rasterizer/c_rasterizer.hpp"

void c_rasterizer::enable(GLenum cap)
{
    glEnable(cap);
}

void c_rasterizer::disable(GLenum cap)
{
    glDisable(cap);
}

void c_rasterizer::depth_mask(GLboolean flag)
{
    glDepthMask(flag);
}

void c_rasterizer::clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    glClearColor(r, g, b, a);
}

void c_rasterizer::clear(GLbitfield mask)
{
    glClear(mask);
}

void c_rasterizer::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glViewport(x, y, width, height);
}

void c_rasterizer::matrix_mode(GLenum mode)
{
    glMatrixMode(mode);
}

void c_rasterizer::load_identity()
{
    glLoadIdentity();
}

void c_rasterizer::ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
                         GLdouble z_near, GLdouble z_far)
{
    glOrtho(left, right, bottom, top, z_near, z_far);
}

void c_rasterizer::rotate(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    glRotatef(angle, x, y, z);
}

void c_rasterizer::translate(GLfloat x, GLfloat y, GLfloat z)
{
    glTranslatef(x, y, z);
}

void c_rasterizer::alpha_func(GLenum func, GLclampf ref)
{
    glAlphaFunc(func, ref);
}

void c_rasterizer::blend_func(GLenum sfactor, GLenum dfactor)
{
    glBlendFunc(sfactor, dfactor);
}

void c_rasterizer::color3f(GLfloat r, GLfloat g, GLfloat b)
{
    glColor3f(r, g, b);
}

void c_rasterizer::color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    glColor4f(r, g, b, a);
}

void c_rasterizer::begin(GLenum mode)
{
    glBegin(mode);
}

void c_rasterizer::end()
{
    glEnd();
}

void c_rasterizer::vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    glVertex3f(x, y, z);
}

void c_rasterizer::vertex2f(GLfloat x, GLfloat y)
{
    glVertex2f(x, y);
}

void c_rasterizer::normal3f(GLfloat x, GLfloat y, GLfloat z)
{
    glNormal3f(x, y, z);
}

void c_rasterizer::tex_coord2f(GLfloat s, GLfloat t)
{
    glTexCoord2f(s, t);
}

void c_rasterizer::push_matrix()
{
    glPushMatrix();
}

void c_rasterizer::pop_matrix()
{
    glPopMatrix();
}

void c_rasterizer::mult_matrix(const GLfloat* m)
{
    glMultMatrixf(m);
}

void c_rasterizer::gen_textures(GLsizei n, GLuint* textures)
{
    glGenTextures(n, textures);
}

void c_rasterizer::bind_texture(GLenum target, GLuint texture)
{
    glBindTexture(target, texture);
}

void c_rasterizer::tex_parameteri(GLenum target, GLenum pname, GLint param)
{
    glTexParameteri(target, pname, param);
}

void c_rasterizer::tex_image_2d(GLenum target, GLint level, GLint internal_format,
                                GLsizei width, GLsizei height, GLint border,
                                GLenum format, GLenum type, const GLvoid* pixels)
{
    glTexImage2D(target, level, internal_format, width, height, border,
                 format, type, pixels);
}

void c_rasterizer::tex_envi(GLenum target, GLenum pname, GLint param)
{
    glTexEnvi(target, pname, param);
}

c_rasterizer::uint_t c_rasterizer::gen_lists(GLsizei range)
{
    return glGenLists(range);
}

void c_rasterizer::new_list(GLuint list, GLenum mode)
{
    glNewList(list, mode);
}

void c_rasterizer::end_list()
{
    glEndList();
}

void c_rasterizer::call_list(GLuint list)
{
    glCallList(list);
}

void c_rasterizer::light_modeli(GLenum pname, GLint param)
{
    glLightModeli(pname, param);
}

void c_rasterizer::light_modelfv(GLenum pname, const GLfloat* params)
{
    glLightModelfv(pname, params);
}

void c_rasterizer::light_fv(GLenum light, GLenum pname, const GLfloat* params)
{
    glLightfv(light, pname, params);
}

void c_rasterizer::color_material(GLenum face, GLenum mode)
{
    glColorMaterial(face, mode);
}