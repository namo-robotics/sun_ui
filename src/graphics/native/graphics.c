/** Executes retained drawing packets without retaining borrowed Sun storage. */
#include "graphics.h"
#include <epoxy/gl.h>
#include <glib.h>
#include <math.h>
#include <string.h>
#ifdef __APPLE__
#include <pthread.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif
/** Owns immutable packed positions and element indices. */
struct GuMesh {
  int refs;
  GArray *vertices;
  GArray *indices;
};
/** Owns immutable shader source strings. */
struct GuShader {
  int refs;
  char *vertex;
  char *fragment;
};
/** Owns a uniform name and copied numeric payload. */
typedef struct {
  char *name;
  int count;
  float values[16];
} Uniform;
/** Retains the CPU resources and per-draw state for one command. */
typedef struct {
  GuMesh *mesh;
  GuShader *shader;
  int mode, depth, blend;
  GArray *uniforms;
} Draw;
/** Owns the clear color and ordered commands. */
struct GuFrame {
  float color[4];
  GArray *draws;
};
/** Associates immutable CPU geometry with objects in exactly one GL context. */
typedef struct {
  GuMesh *mesh;
  GLuint vao, vbo, ebo;
  int used;
} MeshCache;
/** Associates a shader with a program in exactly one GL context. */
typedef struct {
  GuShader *shader;
  GLuint program;
  int used;
} ShaderCache;
/** Carries input independently of widget reconciliation events. */
typedef struct {
  int kind, code;
  double x, y;
} Input;
/** Owns rendering state and input storage for one host surface. */
struct GuSurface {
  int refs, frames, width, height, readback;
  unsigned char pixel[4];
  GuFrame *frame;
  GArray *meshes, *shaders;
  GQueue events;
  Input last;
  char error[2048];
};
static float uniform_builder[16];
int gu_text_valid(const char *text, uint64_t length) {
  return strlen(text) == length && g_utf8_validate(text, length, NULL);
}
void gu_thread(void) {
#ifdef __APPLE__
  if (!pthread_main_np())
    g_error("Graphics access requires the main thread");
#else
  if (syscall(SYS_gettid) != getpid())
    g_error("Graphics access requires the main thread");
#endif
}
GuMesh *gu_mesh_new(int vertices, int indices) {
  gu_thread();
  if (vertices <= 0 || vertices % 3 || indices < 0)
    return NULL;
  GuMesh *m = g_new0(GuMesh, 1);
  m->refs = 1;
  m->vertices = g_array_sized_new(FALSE, TRUE, sizeof(float), vertices);
  m->indices = g_array_sized_new(FALSE, TRUE, sizeof(unsigned), indices);
  g_array_set_size(m->vertices, vertices);
  g_array_set_size(m->indices, indices);
  return m;
}
void gu_mesh_vertex(GuMesh *m, int i, float v) {
  gu_thread();
  g_array_index(m->vertices, float, i) = v;
}
void gu_mesh_index(GuMesh *m, int i, unsigned v) {
  gu_thread();
  g_array_index(m->indices, unsigned, i) = v;
}
void gu_mesh_free(GuMesh *m) {
  gu_thread();
  if (m && --m->refs == 0) {
    g_array_unref(m->vertices);
    g_array_unref(m->indices);
    g_free(m);
  }
}
GuShader *gu_shader_new(const char *v, const char *f) {
  gu_thread();
  GuShader *s = g_new0(GuShader, 1);
  s->refs = 1;
  s->vertex = g_strdup(v);
  s->fragment = g_strdup(f);
  return s;
}
void gu_shader_free(GuShader *s) {
  gu_thread();
  if (s && --s->refs == 0) {
    g_free(s->vertex);
    g_free(s->fragment);
    g_free(s);
  }
}
GuFrame *gu_frame_new(float r, float g, float b, float a) {
  gu_thread();
  GuFrame *f = g_new0(GuFrame, 1);
  f->color[0] = r;
  f->color[1] = g;
  f->color[2] = b;
  f->color[3] = a;
  f->draws = g_array_new(FALSE, TRUE, sizeof(Draw));
  return f;
}
void gu_frame_free(GuFrame *f) {
  gu_thread();
  if (!f)
    return;
  for (guint i = 0; i < f->draws->len; i++) {
    Draw *d = &g_array_index(f->draws, Draw, i);
    gu_mesh_free(d->mesh);
    gu_shader_free(d->shader);
    for (guint j = 0; j < d->uniforms->len; j++)
      g_free(g_array_index(d->uniforms, Uniform, j).name);
    g_array_unref(d->uniforms);
  }
  g_array_unref(f->draws);
  g_free(f);
}
int gu_frame_draw(GuFrame *f, GuMesh *m, GuShader *s, int mode, int depth,
                  int blend) {
  gu_thread();
  guint count = m->indices->len ? m->indices->len : m->vertices->len / 3;
  if ((mode != 0 && mode != 1) || count % (mode ? 2 : 3))
    return 0;
  for (guint i = 0; i < m->vertices->len; i++)
    if (!isfinite(g_array_index(m->vertices, float, i)))
      return 0;
  for (guint i = 0; i < m->indices->len; i++)
    if (g_array_index(m->indices, unsigned, i) >= m->vertices->len / 3)
      return 0;
  Draw d = {m,     s,     mode,
            depth, blend, g_array_new(FALSE, TRUE, sizeof(Uniform))};
  m->refs++;
  s->refs++;
  g_array_append_val(f->draws, d);
  return 1;
}
int gu_frame_uniform(GuFrame *f, const char *name, int count,
                     const float *values) {
  gu_thread();
  if (!f->draws->len || !*name ||
      !(count == 1 || count == 2 || count == 3 || count == 4 || count == 16))
    return 0;
  for (int i = 0; i < count; i++)
    if (!isfinite(values[i]))
      return 0;
  Uniform u = {g_strdup(name), count, {0}};
  memcpy(u.values, values, count * sizeof(float));
  Draw *d = &g_array_index(f->draws, Draw, f->draws->len - 1);
  g_array_append_val(d->uniforms, u);
  return 1;
}
void gu_uniform_value(int i, float v) {
  gu_thread();
  if (i >= 0 && i < 16)
    uniform_builder[i] = v;
}
int gu_frame_uniform_built(GuFrame *f, const char *n, int c) {
  return gu_frame_uniform(f, n, c, uniform_builder);
}
GuSurface *gu_surface_new(void) {
  gu_thread();
  GuSurface *s = g_new0(GuSurface, 1);
  s->refs = 1;
  s->meshes = g_array_new(FALSE, TRUE, sizeof(MeshCache));
  s->shaders = g_array_new(FALSE, TRUE, sizeof(ShaderCache));
  g_queue_init(&s->events);
  return s;
}
void gu_surface_ref(void *p) {
  gu_thread();
  ((GuSurface *)p)->refs++;
}
void gu_surface_reset(void *p, int live) {
  gu_thread();
  GuSurface *s = p;
  for (guint i = 0; i < s->meshes->len; i++) {
    MeshCache *m = &g_array_index(s->meshes, MeshCache, i);
    if (live) {
      glDeleteBuffers(1, &m->vbo);
      glDeleteBuffers(1, &m->ebo);
      glDeleteVertexArrays(1, &m->vao);
    }
    gu_mesh_free(m->mesh);
  }
  for (guint i = 0; i < s->shaders->len; i++) {
    ShaderCache *c = &g_array_index(s->shaders, ShaderCache, i);
    if (live)
      glDeleteProgram(c->program);
    gu_shader_free(c->shader);
  }
  g_array_set_size(s->meshes, 0);
  g_array_set_size(s->shaders, 0);
}
void gu_surface_free(void *p) {
  gu_thread();
  GuSurface *s = p;
  if (!s || --s->refs)
    return;
  gu_surface_reset(s, 0);
  gu_frame_free(s->frame);
  g_array_unref(s->meshes);
  g_array_unref(s->shaders);
  g_queue_clear_full(&s->events, g_free);
  g_free(s);
}
void gu_surface_submit(GuSurface *s, GuFrame *f) {
  gu_thread();
  GuFrame *copy =
      gu_frame_new(f->color[0], f->color[1], f->color[2], f->color[3]);
  for (guint i = 0; i < f->draws->len; i++) {
    Draw *d = &g_array_index(f->draws, Draw, i);
    gu_frame_draw(copy, d->mesh, d->shader, d->mode, d->depth, d->blend);
    for (guint j = 0; j < d->uniforms->len; j++) {
      Uniform *u = &g_array_index(d->uniforms, Uniform, j);
      gu_frame_uniform(copy, u->name, u->count, u->values);
    }
  }
  gu_frame_free(s->frame);
  s->frame = copy;
  s->error[0] = 0;
}
/** Compiles one stage and records diagnostics without leaking a failed shader.
 */
static GLuint compile(GuSurface *s, GLenum kind, const char *source) {
  GLuint id = glCreateShader(kind);
  glShaderSource(id, 1, &source, NULL);
  glCompileShader(id);
  GLint ok;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glGetShaderInfoLog(id, sizeof(s->error), NULL, s->error);
    glDeleteShader(id);
    return 0;
  }
  return id;
}
/** Returns a cached context-local program or compiles it on demand. */
static GLuint program(GuSurface *s, GuShader *shader) {
  for (guint i = 0; i < s->shaders->len; i++) {
    ShaderCache *c = &g_array_index(s->shaders, ShaderCache, i);
    if (c->shader == shader) {
      c->used = 1;
      return c->program;
    }
  }
  GLuint v = compile(s, GL_VERTEX_SHADER, shader->vertex);
  if (!v)
    return 0;
  GLuint f = compile(s, GL_FRAGMENT_SHADER, shader->fragment);
  if (!f) {
    glDeleteShader(v);
    return 0;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    glGetProgramInfoLog(p, sizeof(s->error), NULL, s->error);
    glDeleteProgram(p);
    return 0;
  }
  ShaderCache c = {shader, p, 1};
  shader->refs++;
  g_array_append_val(s->shaders, c);
  return p;
}
/** Uploads immutable geometry once per context. */
static GLuint geometry(GuSurface *s, GuMesh *mesh) {
  for (guint i = 0; i < s->meshes->len; i++) {
    MeshCache *m = &g_array_index(s->meshes, MeshCache, i);
    if (m->mesh == mesh) {
      m->used = 1;
      return m->vao;
    }
  }
  MeshCache m = {mesh, 0, 0, 0, 1};
  mesh->refs++;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glGenBuffers(1, &m.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->vertices->len * sizeof(float),
               mesh->vertices->data, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
  if (mesh->indices->len) {
    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indices->len * sizeof(unsigned),
                 mesh->indices->data, GL_STATIC_DRAW);
  }
  g_array_append_val(s->meshes, m);
  return m.vao;
}
/** Evicts resources absent from the latest frame while the owning context is
 * current. */
static void prune(GuSurface *s) {
  for (guint i = s->meshes->len; i > 0; i--) {
    MeshCache *m = &g_array_index(s->meshes, MeshCache, i - 1);
    if (!m->used) {
      glDeleteBuffers(1, &m->vbo);
      glDeleteBuffers(1, &m->ebo);
      glDeleteVertexArrays(1, &m->vao);
      gu_mesh_free(m->mesh);
      g_array_remove_index(s->meshes, i - 1);
    }
  }
  for (guint i = s->shaders->len; i > 0; i--) {
    ShaderCache *c = &g_array_index(s->shaders, ShaderCache, i - 1);
    if (!c->used) {
      glDeleteProgram(c->program);
      gu_shader_free(c->shader);
      g_array_remove_index(s->shaders, i - 1);
    }
  }
}
int gu_surface_render(void *p, int w, int h) {
  gu_thread();
  GuSurface *s = p;
  s->width = w;
  s->height = h;
  if (w <= 0 || h <= 0 || !s->frame)
    return 1;
  if (!epoxy_is_desktop_gl() || epoxy_gl_version() < 33) {
    g_strlcpy(s->error, "Desktop OpenGL 3.3 is required", sizeof(s->error));
    return 0;
  }
  s->error[0] = 0;
  for (guint i = 0; i < s->meshes->len; i++)
    g_array_index(s->meshes, MeshCache, i).used = 0;
  for (guint i = 0; i < s->shaders->len; i++)
    g_array_index(s->shaders, ShaderCache, i).used = 0;
  glViewport(0, 0, w, h);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glClearColor(s->frame->color[0], s->frame->color[1], s->frame->color[2],
               s->frame->color[3]);
  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  for (guint i = 0; i < s->frame->draws->len; i++) {
    Draw *d = &g_array_index(s->frame->draws, Draw, i);
    GLuint prog = program(s, d->shader);
    if (!prog) {
      prune(s);
      return 0;
    }
    glUseProgram(prog);
    glBindVertexArray(geometry(s, d->mesh));
    if (d->depth)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    if (d->blend)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (guint j = 0; j < d->uniforms->len; j++) {
      Uniform *u = &g_array_index(d->uniforms, Uniform, j);
      GLint loc = glGetUniformLocation(prog, u->name);
      if (loc < 0)
        continue;
      switch (u->count) {
      case 1:
        glUniform1fv(loc, 1, u->values);
        break;
      case 2:
        glUniform2fv(loc, 1, u->values);
        break;
      case 3:
        glUniform3fv(loc, 1, u->values);
        break;
      case 4:
        glUniform4fv(loc, 1, u->values);
        break;
      case 16:
        glUniformMatrix4fv(loc, 1, GL_FALSE, u->values);
        break;
      }
    }
    GLenum mode = d->mode ? GL_LINES : GL_TRIANGLES;
    if (d->mesh->indices->len)
      glDrawElements(mode, d->mesh->indices->len, GL_UNSIGNED_INT, NULL);
    else
      glDrawArrays(mode, 0, d->mesh->vertices->len / 3);
  }
  glBindVertexArray(0);
  glUseProgram(0);
  prune(s);
  if (s->readback)
    glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, s->pixel);
  GLenum error = glGetError();
  if (error) {
    g_snprintf(s->error, sizeof(s->error), "OpenGL error 0x%x", error);
    return 0;
  }
  s->frames++;
  return 1;
}
const char *gu_surface_error(GuSurface *s) {
  gu_thread();
  return s->error;
}
int gu_surface_frames(GuSurface *s) {
  gu_thread();
  return s->frames;
}
int gu_surface_extent(GuSurface *s, int horizontal) {
  gu_thread();
  return horizontal ? s->width : s->height;
}
int gu_surface_pixel(GuSurface *s, int c) {
  gu_thread();
  return c >= 0 && c < 4 ? s->pixel[c] : 0;
}
void gu_surface_readback(GuSurface *s, int enabled) {
  gu_thread();
  s->readback = enabled;
}
void gu_surface_input(void *p, int kind, int code, double x, double y) {
  gu_thread();
  GuSurface *s = p;
  if (s->events.length >= 4096) {
    g_queue_clear_full(&s->events, g_free);
    Input *reset = g_new0(Input, 1);
    reset->kind = 5;
    g_queue_push_tail(&s->events, reset);
  }
  Input *e = g_new(Input, 1);
  *e = (Input){kind, code, x, y};
  g_queue_push_tail(&s->events, e);
}
int gu_surface_next(GuSurface *s) {
  gu_thread();
  Input *e = g_queue_pop_head(&s->events);
  if (!e)
    return 0;
  s->last = *e;
  g_free(e);
  return 1;
}
double gu_surface_event(GuSurface *s, int f) {
  gu_thread();
  return f == 0   ? s->last.kind
         : f == 1 ? s->last.code
         : f == 2 ? s->last.x
                  : s->last.y;
}
