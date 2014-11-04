
#include <node.h>
#include <v8.h>

#include <node_buffer.h>
#include <node_object_wrap.h>

#include <FreeImagePlus.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <math.h>


using namespace node;
using namespace v8;

class RemoveSkin;

struct Baton {
	uv_work_t request;
	Persistent<Function> callback;

	RemoveSkin* obj;

	FIMEMORY *fiMemoryOut;
	FIMEMORY *fiMemoryIn;
	/*
	const char* imageBuffer;
	size_t imageBufferLength;

	const char* imageResult;
	size_t imageResultLength;
	*/
};

struct range {
	float s0;
	float s1;
	float v0;
	float v1;
};

void RGBtoHSV( uint8_t ir, uint8_t ig, uint8_t ib, float *h, float *s, float *v )
{

	float r = ((float)ir)/0xff, g = ((float)ig)/0xff, b = ((float)ib)/0xff;

	float min, max, delta;
	min = fminf( fminf( r, g ), b );
	max = fmaxf( fmaxf( r, g ), b );
	*v = max;				// v
	delta = max - min;
	if( max != 0 )
		*s = delta / max;		// s
	else {
		// r = g = b = 0		// s = 0, v is undefined
		*s = 0;
		//*h = -1;
		//return;
	}
	if(max == min){
		*h = 0;
	}else{

		if( r == max )
			*h = ( g - b ) / delta + (g < b ? 6 : 0);	// between yellow & magenta
		else if( g == max )
			*h = ( b - r ) / delta + 2;	// between cyan & yellow
		else
			*h = ( r - g ) / delta + 4;	// between magenta & cyan

		*h *= 60;				// degrees
	}
}

static Persistent<FunctionTemplate> constructor;

class RemoveSkin: public ObjectWrap {
public:

	RemoveSkin() {}
	~RemoveSkin() {}

	struct range tableSkin[361];

	bool isSkin(uint8_t r,uint8_t g,uint8_t b){
		float o = 0.10;
		float h ,s ,v;
		RGBtoHSV(r ,g ,b ,&h ,&s ,&v );

		struct range* cr = &tableSkin[((uint)h)%360];

		return (cr->s0 != -1 || cr->v0 != -1) &&
				cr->s0 <= s + o && cr->s1 >= s - o  &&
				cr->v0 <= v + o && cr->v1 >= v - o ;
	}

	static Handle<Value> New(const Arguments& args) {
		RemoveSkin* obj = new RemoveSkin();

		int i;

		if (args.Length() < 1) {
					return ThrowException(Exception::TypeError(String::New("Expecting 1 arguments")));
		}

		if (!Buffer::HasInstance(args[0])) {
			return ThrowException(Exception::TypeError(String::New("First argument must be a Buffer")));
		}

#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION < 10
		Local < Object > buffer_obj = args[0]->ToObject();
#else
		Local<Value> buffer_obj = args[0];
#endif


		FIMEMORY * fiMemoryIn = FreeImage_OpenMemory((BYTE *)Buffer::Data(buffer_obj),Buffer::Length(buffer_obj));

		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(fiMemoryIn);

		FIBITMAP * fiBitmap = FreeImage_LoadFromMemory(format, fiMemoryIn);

		if(FreeImage_GetBPP(fiBitmap) != 32)
			return ThrowException(Exception::TypeError(String::New("not have alpha channel")));

		BYTE * bits = FreeImage_GetBits(fiBitmap);

		int bitsLength = FreeImage_GetWidth(fiBitmap) * FreeImage_GetHeight(fiBitmap) * 4;


		for(i = 0;i<360;i++)
			obj->tableSkin[i].v0 = obj->tableSkin[i].v1 = obj->tableSkin[i].s0 = obj->tableSkin[i].s1 = -1;



		for (i = 0; i < bitsLength; i += 4) {
			uint8_t b = bits[i];
			uint8_t g = bits[i+1];
			uint8_t r = bits[i+2];
			uint8_t a = bits[i+3];
			if(a == 0xff &&  !(r == 0xff && g ==0xff && b ==0xff) ){
				float h ,s ,v;

				RGBtoHSV(r ,g ,b ,&h ,&s ,&v );

				struct range* currentRange = &obj->tableSkin[((uint)h)%360];

				if(currentRange->s0 == -1){
					currentRange->s0 = currentRange->s1 = s;
				}else{
					currentRange->s0 = fminf(s,currentRange->s0);
					currentRange->s1 = fmaxf(s,currentRange->s1);
				}

				if(currentRange->v0 == -1){
					currentRange->v0 = currentRange->v1 = v;
				}else{
					currentRange->v0 = fminf(v,currentRange->v0);
					currentRange->v1 = fmaxf(v,currentRange->v1);
				}
			}
		}


		FreeImage_Unload(fiBitmap);

		obj->Wrap(args.This());
		return args.This();
	}
	static Handle<Value> getList(const Arguments& args) {
		HandleScope scope;
		RemoveSkin* obj = ObjectWrap::Unwrap < RemoveSkin > (args.This());

		Handle<Array> array = Array::New(360);

		for(int i =0; i<360;i++){
			Handle<Array> subarray = Array::New(4);
			struct range* currentRange = &obj->tableSkin[i];
			subarray->Set(0, Number::New(currentRange->s0));
			subarray->Set(1, Number::New(currentRange->s1));
			subarray->Set(2, Number::New(currentRange->v0));
			subarray->Set(3, Number::New(currentRange->v1));
			array->Set(i, subarray);
		}


		return scope.Close(array);

	}
	static Handle<Value> removeSkin(const Arguments& args) {
		HandleScope scope;
		RemoveSkin* obj = ObjectWrap::Unwrap < RemoveSkin > (args.This());

		if (args.Length() < 2) {
			return ThrowException(Exception::TypeError(String::New("Expecting 2 arguments")));
		}

		if (!Buffer::HasInstance(args[0])) {
			return ThrowException(Exception::TypeError(String::New("First argument must be a Buffer")));
		}

		if (!args[1]->IsFunction()) {
			return ThrowException(Exception::TypeError(String::New("Second argument must be a RemoveSkinllback function")));
		}

		Local < Function > callback = Local < Function > ::Cast(args[1]);

#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION < 10
		Local < Object > buffer_obj = args[0]->ToObject();
#else
		Local<Value> buffer_obj = args[0];
#endif



		Baton* baton = new Baton();
		baton->request.data = baton;
		baton->callback = Persistent < Function > ::New(callback);
		baton->obj = obj;

		/*
		baton->imageBuffer = Buffer::Data(buffer_obj);
		baton->imageBufferLength = Buffer::Length(buffer_obj);

		baton->imageResult = NULL;
		baton->imageResultLength = 0;
		*/
		baton->fiMemoryIn = FreeImage_OpenMemory((BYTE *)Buffer::Data(buffer_obj),Buffer::Length(buffer_obj));
		baton->fiMemoryOut = NULL;

		int status = uv_queue_work(uv_default_loop(),
		&baton->request,
		RemoveSkin::DetectWork,
		(uv_after_work_cb)RemoveSkin::DetectAfter);


		assert(status == 0);
		return Undefined();
	}

	static void DetectWork(uv_work_t* req) {

		Baton* baton = static_cast<Baton*>(req->data);
		uint i;
		RemoveSkin* obj = baton->obj;

		FIMEMORY * fiMemoryIn = baton->fiMemoryIn;//FreeImage_OpenMemory((BYTE *)baton->imageBuffer,baton->imageBufferLength);
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(fiMemoryIn);



		FIBITMAP * fiBitmap = FreeImage_LoadFromMemory(format, fiMemoryIn);
		FreeImage_CloseMemory(fiMemoryIn);
		//FIBITMAP * thumbnail = FreeImage_MakeThumbnail(fiBitmap, 32, TRUE);

		//RGBQUAD rgba;
		//int w = FreeImage_GetWidth(fiBitmap);
		int h = FreeImage_GetHeight(fiBitmap);
/*
 * for(int y =0;y<h;y++){
			for(int x =0;x<w;x++){
				FreeImage_GetPixelColor(fiBitmap,x,y,&rgba);
				if(obj->isSkin(rgba.rgbRed , rgba.rgbGreen , rgba.rgbBlue)){
					rgba.rgbBlue = rgba.rgbGreen=rgba.rgbRed=rgba.rgbReserved = 0xff;
					FreeImage_SetPixelColor(fiBitmap,x,y,&rgba);
				}
			}
		}
*/
		//uint8_t * bits = (uint8_t *)FreeImage_GetBits(fiBitmap);
		uint bpp = FreeImage_GetBPP(fiBitmap);
		//uint bitsLength = FreeImage_GetWidth(fiBitmap) * FreeImage_GetHeight(fiBitmap) * (bpp/8);
		//uint bitsLength = (FreeImage_GetPitch(fiBitmap) * (FreeImage_GetHeight(fiBitmap))) ;
		uint pitch = FreeImage_GetPitch(fiBitmap);

		//printf("bpp %d\n",bpp);

		if(bpp == 32){
			for(int y =0; y<h ;y++){
				BYTE *bitss = FreeImage_GetScanLine(fiBitmap, y);
				for (i = 0; i < pitch; i += 4) {
					if(obj->isSkin(bitss[i+2] , bitss[i+1] , bitss[i]))
						bitss[i] = bitss[i+1] = bitss[i+2] = bitss[i+3] = 0;
				}
			}
		}
		if(bpp == 24){
			for(int y =0;y<h;y++){
				BYTE *bitss = FreeImage_GetScanLine(fiBitmap, y);
				for (i = 0; i < pitch; i += 3) {
					if(obj->isSkin( bitss[i+2], bitss[i+1] ,bitss[i] )){
						bitss[i] = bitss[i+1] = bitss[i+2] = 0xff;
					}
				}
			}
		}

		FIMEMORY * fiMemoryOut = FreeImage_OpenMemory();

		FreeImage_SaveToMemory(format, fiBitmap, fiMemoryOut, 0);

		FreeImage_Unload(fiBitmap);

		baton->fiMemoryOut =  fiMemoryOut;

		//FreeImage_Unload(thumbnail);


		/*
		fipMemoryIO* memin = new fipMemoryIO((BYTE*)baton->imageBuffer,baton->imageBufferLength);
		FREE_IMAGE_FORMAT format = fipImage::identifyFIFFromMemory((FIMEMORY*)memin);

		fipImage* image = new fipImage();

		image->loadFromMemory((fipMemoryIO&)memin);

		fipMemoryIO* memout = new fipMemoryIO();

		image->saveToMemory(format,(fipMemoryIO&)memout);

		memout->acquire((BYTE**)&baton->imageResult, (DWORD*)&baton->imageResultLength);
		*/
	}

	static void DetectAfter(uv_work_t* req) {
		HandleScope scope;
		Baton* baton = static_cast<Baton*>(req->data);

		if (baton->fiMemoryOut) {
			const unsigned argc = 2;
			const char*data;
			int datalen;
			FreeImage_AcquireMemory(baton->fiMemoryOut,(BYTE**)&data, (DWORD*)&datalen );
			Local<Value> argv[argc] = {
				Local<Value>::New( Null() ),
				Local<Object>::New( Buffer::New(data,datalen)->handle_)
			};

			TryCatch try_catch;
			baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
			if (try_catch.HasCaught())
				FatalException(try_catch);
		} else {
			Local < Value > err = Exception::Error(
					String::New("error"));

			const unsigned argc = 1;
			Local<Value> argv[argc] = {err};

			TryCatch try_catch;
			baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
			if (try_catch.HasCaught())
				FatalException(try_catch);
		}

		if(baton->fiMemoryOut)
			FreeImage_CloseMemory(baton->fiMemoryOut);


		baton->callback.Dispose();
		delete baton;
	}

	static void Initialize(Handle<Object> target) {
		HandleScope scope;

		Local < FunctionTemplate > tpl = FunctionTemplate::New(New);
		Local < String > name = String::NewSymbol("RemoveSkin");

		constructor = Persistent < FunctionTemplate > ::New(tpl);
		constructor->InstanceTemplate()->SetInternalFieldCount(1);
		constructor->SetClassName(name);


		NODE_SET_PROTOTYPE_METHOD(constructor, "removeSkin", removeSkin);
		NODE_SET_PROTOTYPE_METHOD(constructor, "getList", getList);

		target->Set(name, constructor->GetFunction());

	}
};

extern "C" {
	void init(Handle<Object> target) {
		HandleScope scope;
		RemoveSkin::Initialize(target);
	}

	NODE_MODULE(removeskin, init);
}
