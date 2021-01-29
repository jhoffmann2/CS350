#include "GBuffer.h"

#include <cassert>
#include <numeric>
#include <imgui/imgui.h>



#include "shader.hpp"
#include "SolidRender.h"

void GBuffer::Init(size_t width, size_t height)
{
  GBuffer& instance = Instance();
  instance.width = width;
  instance.height = height;

  GLuint &gBufferFBO = instance.gBufferFBO;
  GLuint &depthTexture = instance.depthTexture;
  GLuint &rboDepth = instance.rboDepth;

  std::array<GLuint, RenderTargetCount> &textures = instance.textures;
  std::array<GLenum, RenderTargetCount> &drawBuffers = instance.drawBuffers;
  
  // create the FBO
  glGenFramebuffers(1, &gBufferFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

  // create the gbuffer textures
  glGenTextures(RenderTargetCount, textures.data());
  glGenTextures(1, &depthTexture);

  for (size_t i = 0; i < RenderTargetCount; ++i)
  {
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_RGBA32F,
      width,
      height,
      0,
      GL_RGBA,
      GL_FLOAT,
      nullptr
    );

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i], 0);
  }

  glGenRenderbuffers(1, &rboDepth);
  glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

  std::iota(drawBuffers.begin(), drawBuffers.end(), GL_COLOR_ATTACHMENT0);
  glDrawBuffers(RenderTargetCount, drawBuffers.data());

  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  instance.SetupFSQ();
}

void GBuffer::Bind()
{
  GBuffer& instance = Instance();

  GLuint& gBufferFBO = instance.gBufferFBO;
  SolidRender::SHADER& unbindShader = instance.unbindShader_;

  unbindShader = SolidRender::GetShader();
  SolidRender::SetShader(SolidRender::DEFFERED);

  glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

}

void GBuffer::UnBind()
{
  GBuffer& instance = Instance();

  GLuint& gBufferFBO = instance.gBufferFBO;
  SolidRender::SHADER& unbindShader = instance.unbindShader_;
  size_t& width = instance.width;
  size_t& height = instance.height;

  SolidRender::SetShader(unbindShader);

  // copy gbuffer depth info over to default FBO
  glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(
    0, 0, width, height,
    0, 0, width, height,
    GL_DEPTH_BUFFER_BIT,
    GL_NEAREST
  );

  // bind default FBO to screen
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::RenderFSQ()
{
  auto& instance = Instance();
  auto &FSQ = instance.FSQ;
  glUseProgram(FSQ.shader_program);

  // load textures
  for(int i = 0; i < RenderTargetCount; ++i)
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, instance.textures[i]);
    glUniform1i(FSQ.uTex[i], i);
  }

  // references to inside of data to avoid rewriting code
  GLuint& vao = FSQ.vertexArrayBuffer_;
  int& face_count = FSQ.faceCount_;

  // turn off writing to depth buffer
  glDepthMask(GL_FALSE);

  // recall state
  glBindVertexArray(vao);
  glLineWidth(3.0f);
  glDrawElements(GL_TRIANGLES, 3 * face_count, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);


  // turn depth buffer back on
  glDepthMask(GL_TRUE);
}

GBuffer& GBuffer::Instance()
{
  static GBuffer buffer;
  return buffer;
}

void GBuffer::SetupFSQ()
{
  FSQ.shader_program = LoadShaders("../../Common/shaders/Deferred/DeferredPhong.vert",
    "../../Common/shaders/Deferred/DeferredPhong.frag");

  static const std::array<glm::vec4, 6> vertices = { {
    {-1.0f, -1.0f, 0.0f, 1},
    {1.0f, -1.0f, 0.0f, 1},
    {1.0f,  1.0f, 0.0f, 1},
    {-1.0f,  1.0f, 0.0f, 1},
  } };

  static const std::array<glm::vec2, 6> vertex_uv = { {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f,  1.0f},
    {0.0f,  1.0f},
  } };

  static const std::array<glm::uvec3, 2> faces = { {
    {0,1,2},
    {0,2,3}
  } };

  // references to inside of data to avoid rewriting code
  GLuint& vertex_buffer = FSQ.bufferObjects_[FSQ_DATA::VERT];
  GLuint& face_buffer = FSQ.bufferObjects_[FSQ_DATA::FACE];
  GLuint& uv_buffer = FSQ.bufferObjects_[FSQ_DATA::UV];
  GLuint& vao = FSQ.vertexArrayBuffer_;
  int& face_count = FSQ.faceCount_;

  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * vertices.size(),
    vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &uv_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vertex_uv.size(),
    vertex_uv.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &face_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, face_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
    sizeof(glm::uvec3) * faces.size(),
    faces.data(), GL_STATIC_DRAW);

  // create vertex array object
  glGenVertexArrays(1, &vao);
  //   start state recording
  glBindVertexArray(vao);

  // assign postion attribute to vertex_buffer
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  const GLint aposition = glGetAttribLocation(FSQ.shader_program, "position");
  glVertexAttribPointer(aposition, 4, GL_FLOAT, false, 0, nullptr);
  glEnableVertexAttribArray(aposition);

  // assign uv attribute to uv_buffer
  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  const GLint auv = glGetAttribLocation(FSQ.shader_program, "uv_vert");
  glVertexAttribPointer(auv, 2, GL_FLOAT, false, 0, nullptr);
  glEnableVertexAttribArray(auv);

  // ready the face buffer for drawing
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, face_buffer);
  //   stop state recording
  glBindVertexArray(0);

  face_count = faces.size();

  FSQ.uTex[TARGET_DIFFUSE] = glGetUniformLocation(FSQ.shader_program, "diffuseTex");
  FSQ.uTex[TARGET_NORMAL] = glGetUniformLocation(FSQ.shader_program, "normalTex");
  FSQ.uTex[TARGET_WORLD_POS] = glGetUniformLocation(FSQ.shader_program, "worldPosTex");

}