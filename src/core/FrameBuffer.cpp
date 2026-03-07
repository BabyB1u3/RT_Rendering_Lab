
// The standard C++ libs
#include <iostream>

#include "FrameBuffer.h"
#include "ResourceManager.h"

FrameBuffer::FrameBuffer()
{
	glGenFramebuffers(1, &this->ID);
}

FrameBuffer::~FrameBuffer()
{

}

void FrameBuffer::Configure(GLuint texture, GLuint type, GLuint rbo)
{
	glBindFramebuffer(GL_FRAMEBUFFER, this->ID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, type, GL_TEXTURE_2D, texture, 0);

	if (rbo == NULL)
	{
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::BeginRender()
{
	glBindFramebuffer(GL_FRAMEBUFFER, this->ID);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void FrameBuffer::EngRender()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}