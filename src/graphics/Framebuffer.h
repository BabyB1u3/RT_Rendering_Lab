enum class FBOTextureFormat
{
	None = 0,
	// Color
	RGBA8,

	// Depth/stencil
	DEPTH24STENCIL8,

	// Defaults

	Depth = DEPTH24STENCIL8

};

struct FBOTextureSpecification
{
	FBOTextureSpecification() = default;
	FBOTextureSpecification(FBOTextureFormat format)
		: TextureFormat(format) {};

	FBOTextureFormat TextureFormat = FBOTextureFormat::None;
};

struct FBOTextureAttachmentSpecification
{
	FBOTextureAttachmentSpecification() = default;
	FBOTextureAttachmentSpecification(std::initializer_list<FBOTextureSpecification> attachments)
		: Attachments(attachments) {};

	std::vector<FBOTextureSpecification> Attachments;
};

struct FBOSpecification
{
	uint32_t Samples = 1;
	uint32_t Width = 0, Height = 0;
	FBOTextureAttachmentSpecification Attachments;
};

//--------------------Frame Buffer---------------------
//------------------------ FBO ------------------------
class FBO
{
public:
	FBO(const FBOSpecification &spec);

	~FBO();

	void Bind() const;

	void Unbind() const;

	uint32_t GetColorAttachment(uint32_t index = 0) const { return m_ColorAttachments[index]; }

	void Resize(uint32_t width, uint32_t height);

private:
	uint32_t m_FBO;

	FBOSpecification m_Specification;

	std::vector<FBOTextureSpecification> m_ColorAttachmentSpecifications;
	FBOTextureSpecification m_DepthAttachmentSpecification;

	std::vector<uint32_t> m_ColorAttachments;
	uint32_t m_DepthAttachment;

private:
	void Create();
};
