#ifndef _VID_EFFECT_H
#define _VID_EFFECT_H

struct IDirect3DDevice9;
struct ID3DXEffect;
struct ID3DXBuffer;

class VidEffect
{
public:

				  VidEffect			(IDirect3DDevice9 *_d3dDevice);
				 ~VidEffect			();

	int			Load					(const char *_filename);
	bool		IsValid				() const { return mEffect != NULL; }

	void    Begin					();
	void    End						();

	void    SetParamFloat2(const char *_name, float _x, float _y);
	void    SetParamFloat4(const char *_name, float _x, float _y, float _z, float _w);

private:

	IDirect3DDevice9			*mD3DDevice;
	ID3DXEffect					*mEffect;
	ID3DXBuffer					*mErrorBuffer;
};

#endif // _VID_EFFECT_H
