
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

class ImageThumb;
class ImageCmp;

struct BatonIc {
	uv_work_t request;
	Persistent<Function> callback;

	ImageCmp* obj;
	uint8_t *data;
	uint found_id;
	uint near_diff;
	uint near_id;
};

struct Baton {
	uv_work_t request;
	Persistent<Function> callback;

	ImageThumb* obj;

	FIMEMORY* fiMemoryOut;
	FIMEMORY* fiMemoryIn;
	FIBITMAP* bitmap;
	u_int8_t* bits;

};


typedef struct {
	u_int8_t b1:4;
	u_int8_t b2:4;
} bit4;

typedef union
{
	u_int32_t u32;
	struct {
		u_int8_t b1;
		u_int8_t b2;
		u_int8_t b3;
		u_int8_t b4;
	} u8;

	struct {
		bit4 b1;
		bit4 b2;
		bit4 b3;
		bit4 b4;
	} b4;

} cmp32;

struct imageSample {
	u_int32_t id;
	u_int8_t data[1024];
};


uint cmp4bit(void *data1,void *data2){

		uint retdiff = 0;

		cmp32 * u32data1 = (cmp32 *)data1;
		cmp32 * u32data2 = (cmp32 *)data2;

		for(int i=0;i < 256 ;i++){



			if(u32data1->u8.b1 != u32data2->u8.b1)
				retdiff += abs(u32data1->u8.b1 - u32data2->u8.b1);

			if(u32data1->u8.b2 != u32data2->u8.b2)
				retdiff += abs(u32data1->u8.b2 - u32data2->u8.b2);

			if(u32data1->u8.b3 != u32data2->u8.b3)
				retdiff += abs(u32data1->u8.b3 - u32data2->u8.b3);

			if(u32data1->u8.b4 != u32data2->u8.b4)
				retdiff += abs(u32data1->u8.b4 - u32data2->u8.b4);

			///printf("%d: %X = %x \n",i,u32data1->u32,u32data2->u32);

			//if(u32data1->u32 != u32data2->u32){
			/*
				if(u32data1->u8.b1 != u32data2->u8.b1){
					retdiff += abs(u32data1->b4.b1.b1 - u32data2->b4.b1.b1);
					retdiff += abs(u32data1->b4.b1.b2 - u32data2->b4.b1.b2);
				}
				if(u32data1->u8.b2 != u32data2->u8.b2){
					retdiff += abs(u32data1->b4.b2.b1 - u32data2->b4.b2.b1);
					retdiff += abs(u32data1->b4.b2.b2 - u32data2->b4.b2.b2);
				}
				if(u32data1->u8.b3 != u32data2->u8.b3){
					retdiff += abs(u32data1->b4.b3.b1 - u32data2->b4.b3.b1);
					retdiff += abs(u32data1->b4.b3.b2 - u32data2->b4.b3.b2);
				}
				if(u32data1->u8.b4 != u32data2->u8.b4){
					retdiff += abs(u32data1->b4.b4.b1 - u32data2->b4.b4.b1);
					retdiff += abs(u32data1->b4.b4.b2 - u32data2->b4.b4.b2);
				}
			*/
			//}
			u32data1++;
			u32data2++;
		}

		return retdiff;
}

static Persistent<FunctionTemplate> constructor;

class ImageCmp: public ObjectWrap {
public:


	struct imageSample *tableCmp;
	uint position ,length ,maxLength , threshold;

	ImageCmp(uint count,uint threshold) {

		count = count ? count : 50;
		this->tableCmp = NULL;
		this->position =  this->length =  this->maxLength = 0;
		this->threshold = threshold ? threshold : 20;

		this->maxLength = count;
		this->tableCmp = (struct imageSample *) malloc(count * sizeof(struct imageSample));
	}
	~ImageCmp() {
		printf("dispose");
		if(this->tableCmp)
			free(this->tableCmp);
	}

	static Handle<Value> New(const Arguments& args) {

		ImageCmp* obj = new ImageCmp(args[0]->IntegerValue(),args[1]->IntegerValue());

		obj->Wrap(args.This());
		return args.This();
	}
	/// ic.add(buffer,id);
	static Handle<Value> Add(const Arguments& args) {
		HandleScope scope;
		ImageCmp* obj = ObjectWrap::Unwrap < ImageCmp > (args.This());
		if (args.Length() < 2) {
			return ThrowException(Exception::TypeError(String::New("Expecting 2 arguments")));
		}

		if (!Buffer::HasInstance(args[0])) {
			return ThrowException( Exception::TypeError( String::New("First argument must be a Buffer")));
		}


		uint id = args[1]->IntegerValue();

#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION < 10
		Local < Object > buffer_obj = args[0]->ToObject();
#else
		Local<Value> buffer_obj = args[0];
#endif

		if(Buffer::Length(buffer_obj) != 1024){
			return ThrowException( Exception::TypeError( String::New("Buffer::Length() != 1024")));
		}

		uint pos = (obj->position + 1) % obj->maxLength;


		memcpy( obj->tableCmp[pos].data ,Buffer::Data(buffer_obj),1024);
		obj->tableCmp[pos].id = id;
		obj->position = pos;
		obj->length++;

		return Undefined();
	}
	/// ic.find(buffer,callback);
	static Handle<Value> Find(const Arguments& args) {
		ImageCmp* obj = ObjectWrap::Unwrap < ImageCmp > (args.This());

		if (args.Length() < 2)
			return ThrowException(Exception::TypeError(String::New("Expecting 2 arguments")));

		if (!Buffer::HasInstance(args[0]))
			return ThrowException(Exception::TypeError(String::New("First argument must be a Buffer")));

		if (!args[1]->IsFunction())
			return ThrowException(Exception::TypeError(String::New("Second argument must be a ImageThumbllback function")));

		Local < Function > callback = Local < Function > ::Cast(args[1]);

#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION < 10
		Local < Object > buffer_obj = args[0]->ToObject();
#else
		Local<Value> buffer_obj = args[0];
#endif

		if(Buffer::Length(buffer_obj) != 1024)
			return ThrowException( Exception::TypeError( String::New("Buffer::Length() != 1024")));


		BatonIc* baton = new BatonIc();
		baton->request.data = baton;
		baton->callback = Persistent < Function > ::New(callback);
		baton->obj = obj;
		baton->data = (uint8_t *)Buffer::Data(buffer_obj);
		baton->found_id = 0;
		baton->near_diff = 0xffffffff;
		baton->near_id = 0;
		int status = uv_queue_work(uv_default_loop(),
				&baton->request,
				ImageCmp::FindWork,
				(uv_after_work_cb)ImageCmp::FindAfter);

		assert(status == 0);
		return Undefined();
	}



	static void FindWork(uv_work_t* req) {
		BatonIc* baton = static_cast<BatonIc*>(req->data);

		ImageCmp* obj = baton->obj;
		int pos = (int) obj->position;

		uint count = 0;
		while(count < obj->maxLength && count < obj->length ){


			struct imageSample * sample = obj->tableCmp + pos;

			uint diff = cmp4bit(sample->data,baton->data);


			if(diff < baton->near_diff){
				baton->near_diff = diff;
				baton->near_id = sample->id;
			}
			//printf("diff %f\n",((float) diff)/262144);
			if(diff <= obj->threshold){
				//printf("found %d\n",sample->id);
				baton->found_id = sample->id;
				return;
			}

			pos--;
			count++;
			if(pos < 0){
				pos += obj->maxLength;
			}
		}
	}



	static void FindAfter(uv_work_t* req) {
			HandleScope scope;
			BatonIc* baton = static_cast<BatonIc*>(req->data);

			const unsigned argc = 4;
			Local<Value> argv[argc] = {
				Local<Value>::New( Null() ),
				Local<Value>::New( Integer::New(baton->found_id)),
				Local<Value>::New( Integer::New(baton->near_diff)),
				Local<Value>::New( Integer::New(baton->near_id))
			};

			TryCatch try_catch;
			baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
			if (try_catch.HasCaught())
				FatalException(try_catch);

			baton->callback.Dispose();

			delete baton;
	}
	static v8::Handle<Value> GetCount(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
		ImageCmp* obj = ObjectWrap::Unwrap < ImageCmp > (info.Holder());
		return v8::Integer::New(obj->maxLength);
	}

	static v8::Handle<Value> GetBytes(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			return v8::Integer::New(1024);
	}
	static void Initialize(Handle<Object> target) {
		HandleScope scope;

		Local < FunctionTemplate > tpl = FunctionTemplate::New(New);
		Local < String > name = String::NewSymbol("ImageCmp");

		constructor = Persistent < FunctionTemplate > ::New(tpl);
		constructor->InstanceTemplate()->SetInternalFieldCount(1);
		constructor->SetClassName(name);
		NODE_SET_PROTOTYPE_METHOD(constructor, "add", Add);
		NODE_SET_PROTOTYPE_METHOD(constructor, "find", Find);

		constructor->InstanceTemplate()->SetAccessor(String::New("size"), GetCount, NULL);

		target->Set(name, constructor->GetFunction());

		target->SetAccessor(String::New("bytes"), GetBytes, NULL);

	}
};


class ImageThumb: public ObjectWrap {
public:

	ImageThumb() {}
	~ImageThumb() {}


	static Handle<Value> imageThumb(const Arguments& args) {
		HandleScope scope;
		//ImageThumb* obj = ObjectWrap::Unwrap < ImageThumb > (args.This());

		if (args.Length() < 2) {
			return ThrowException(Exception::TypeError(String::New("Expecting 2 arguments")));
		}

		if (!Buffer::HasInstance(args[0])) {
			return ThrowException(Exception::TypeError(String::New("First argument must be a Buffer")));
		}

		if (!args[1]->IsFunction()) {
			return ThrowException(Exception::TypeError(String::New("Second argument must be a ImageThumbllback function")));
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
		//baton->obj = obj;

		/*
		baton->imageBuffer = Buffer::Data(buffer_obj);
		baton->imageBufferLength = Buffer::Length(buffer_obj);

		baton->imageResult = NULL;
		baton->imageResultLength = 0;
		*/
		baton->fiMemoryIn = FreeImage_OpenMemory((BYTE *)Buffer::Data(buffer_obj),Buffer::Length(buffer_obj));
		baton->fiMemoryOut = NULL;
		baton->bitmap = NULL;
		baton->bits = NULL;

		int status = uv_queue_work(uv_default_loop(),
		&baton->request,
		ImageThumb::DetectWork,
		(uv_after_work_cb)ImageThumb::DetectAfter);


		assert(status == 0);
		return Undefined();
	}

	static void DetectWork(uv_work_t* req) {

		Baton* baton = static_cast<Baton*>(req->data);
		//uint i;
		//ImageThumb* obj = baton->obj;

		FIMEMORY * fiMemoryIn = baton->fiMemoryIn;//FreeImage_OpenMemory((BYTE *)baton->imageBuffer,baton->imageBufferLength);
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(fiMemoryIn);



		FIBITMAP * fiBitmap = FreeImage_LoadFromMemory(format, fiMemoryIn);

		//BITMAPINFOHEADER *info = FreeImage_GetInfoHeader(fiBitmap);
		//printf("biSizeImage %d , biCompression %d \n",info->biSizeImage,info->biCompression);

		FreeImage_CloseMemory(fiMemoryIn);

		FIBITMAP * thumbnail1 = FreeImage_Rescale(fiBitmap,32,32,FILTER_BICUBIC);

		FIBITMAP * thumbnail2 = FreeImage_ConvertTo8Bits(thumbnail1);


		uint8_t * bits = (uint8_t *)FreeImage_GetBits(thumbnail2);

		baton->bits = bits;
		baton->bitmap = thumbnail2;

		//FIMEMORY * fiMemoryOut = FreeImage_OpenMemory();

		//FreeImage_SaveToMemory( FIF_BMP, thumbnail2, fiMemoryOut, 0 );

		FreeImage_Unload( fiBitmap );
		FreeImage_Unload( thumbnail1 );
		//FreeImage_Unload( thumbnail2 );

		//baton->fiMemoryOut =  fiMemoryOut;




	}

	static void DetectAfter(uv_work_t* req) {
		HandleScope scope;
		Baton* baton = static_cast<Baton*>(req->data);

		if (baton->bits) {
			const unsigned argc = 2;
			//const char*data;
			//int datalen;
			//FreeImage_AcquireMemory(baton->fiMemoryOut,(BYTE**)&data, (DWORD*)&datalen );
			Local<Value> argv[argc] = {
				Local<Value>::New( Null() ),
				Local<Object>::New( Buffer::New((const char*)baton->bits,1024)->handle_)
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

		if(baton->bitmap)
			FreeImage_Unload( baton->bitmap );

		baton->callback.Dispose();
		delete baton;
	}

	static void Initialize(Handle<Object> target) {
		HandleScope scope;

		target->Set(String::NewSymbol("imageThumb"),
						FunctionTemplate::New(imageThumb)->GetFunction());
	}
};

extern "C" {
	void init(Handle<Object> target) {
		HandleScope scope;
		ImageThumb::Initialize(target);
		ImageCmp::Initialize(target);
	}

	NODE_MODULE(imagecmp, init);
}
