// cc -O2 lpips_ort.c -I/opt/homebrew/include/onnxruntime -L/opt/homebrew/lib -lonnxruntime -o lpips_ort
// Usage (split):
//   ./lpips_ort --diff lpips_diff.onnx --feat lpips_feature.onnx --imgA a.jpg --imgB b.jpg
// Usage (single):
//   ./lpips_ort --model lpips_images.onnx --imgA a.jpg --imgB b.jpg

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "onnxruntime_c_api.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_GIF
#include "stb_image.h"

static const OrtApi* g_api = NULL;

static void ort_ok(OrtStatus* st){
  if (st){
    const char* msg = g_api->GetErrorMessage(st);
    fprintf(stderr, "ONNXRuntime error: %s\n", msg ? msg : "(null)");
    g_api->ReleaseStatus(st);
    exit(1);
  }
}

static int ends_with(const char* s, const char* suf){
  size_t n = strlen(s), m = strlen(suf);
  return (n>=m && memcmp(s+n-m, suf, m)==0);
}

typedef struct { int w,h,c; float* nchw; } ImageF32;

static void free_image(ImageF32* im){ if(im && im->nchw) free(im->nchw); }

static ImageF32 load_to_nchw(const char* path){
  ImageF32 out = (ImageF32){0};
  int w,h,c;
  unsigned char* data = stbi_load(path, &w, &h, &c, 3);
  if(!data){ fprintf(stderr, "Failed to load image: %s\n", path); exit(1); }
  c = 3;
  size_t H=(size_t)h, W=(size_t)w;
  float* nchw = (float*)malloc((size_t)3*H*W*sizeof(float));
  if(!nchw){ fprintf(stderr,"OOM\n"); stbi_image_free(data); exit(1); }
  for(int y=0;y<h;++y){
    for(int x=0;x<w;++x){
      int idx=(y*w+x)*3;
      float r=data[idx+0]/127.5f-1.0f;
      float g=data[idx+1]/127.5f-1.0f;
      float b=data[idx+2]/127.5f-1.0f;
      size_t o=(size_t)y*(size_t)w+(size_t)x;
      nchw[0*(H*W)+o]=r; nchw[1*(H*W)+o]=g; nchw[2*(H*W)+o]=b;
    }
  }
  stbi_image_free(data);
  out.w=w; out.h=h; out.c=c; out.nchw=nchw; return out;
}

static float* bilinear_resize_nchw3(const float* src, int sw, int sh, int dw, int dh){
  float* dst=(float*)malloc((size_t)3*dh*dw*sizeof(float));
  if(!dst){ fprintf(stderr,"OOM\n"); exit(1); }
  for(int c=0;c<3;++c){
    for(int y=0;y<dh;++y){
      float fy=(float)y*(float)sh/(float)dh;
      int y0=(int)fy; float wy=fy-y0; if(y0>=sh-1){ y0=sh-2; wy=1.0f; }
      for(int x=0;x<dw;++x){
        float fx=(float)x*(float)sw/(float)dw;
        int x0=(int)fx; float wx=fx-x0; if(x0>=sw-1){ x0=sw-2; wx=1.0f; }
        size_t o00=(size_t)c*sh*sw + (size_t)y0*sw + (size_t)x0;
        float v=(1-wx)*(1-wy)*src[o00]
               +wx*(1-wy)*src[o00+1]
               +(1-wx)*wy*src[o00+sw]
               +wx*wy*src[o00+sw+1];
        dst[(size_t)c*dh*dw + (size_t)y*dw + (size_t)x]=v;
      }
    }
  }
  return dst;
}

static void get_first_input_shape(OrtSession* s, int64_t* dims, size_t* rank){
  OrtTypeInfo* ti=NULL; ort_ok(g_api->SessionGetInputTypeInfo(s,0,&ti));
  const OrtTensorTypeAndShapeInfo* si=NULL;
  ort_ok(g_api->CastTypeInfoToTensorInfo(ti,&si));
  ort_ok(g_api->GetDimensionsCount(si,rank));
  ort_ok(g_api->GetDimensions(si,dims,*rank));
  g_api->ReleaseTypeInfo(ti);
}

static int starts_with_feat(const char* s){ return strncmp(s,"feat_",5)==0; }
static int tail_index(const char* name){
  int n=(int)strlen(name), i=n-1;
  while(i>=0 && isdigit((unsigned char)name[i])) --i;
  if(i==n-1) return -1;
  return atoi(name+i+1);
}

static char* get_input_name(OrtSession* s, size_t i, OrtAllocator* a){
  char* n=NULL; ort_ok(g_api->SessionGetInputName(s,i,a,&n)); return n;
}
static char* get_output_name(OrtSession* s, size_t i, OrtAllocator* a){
  char* n=NULL; ort_ok(g_api->SessionGetOutputName(s,i,a,&n)); return n;
}

int main(int argc, char** argv){
  const char* model_single = NULL;
  const char* model_diff   = NULL;
  const char* model_feat   = NULL;
  const char* imgA_path    = NULL;
  const char* imgB_path    = NULL;

  for(int i=1;i<argc;i++){
    if(strcmp(argv[i],"--model")==0 && i+1<argc) { model_single = argv[++i]; continue; }
    if(strcmp(argv[i],"--diff")==0  && i+1<argc) { model_diff   = argv[++i]; continue; }
    if(strcmp(argv[i],"--feat")==0  && i+1<argc) { model_feat   = argv[++i]; continue; }
    if(strcmp(argv[i],"--imgA")==0  && i+1<argc) { imgA_path    = argv[++i]; continue; }
    if(strcmp(argv[i],"--imgB")==0  && i+1<argc) { imgB_path    = argv[++i]; continue; }
  }

  if(!(imgA_path && imgB_path) || (!model_single && !model_diff)){
    fprintf(stderr,
      "Usage (split):  %s --diff lpips_diff.onnx --feat lpips_feature.onnx --imgA a.jpg --imgB b.jpg\n"
      "Usage (single): %s --model lpips_images.onnx --imgA a.jpg --imgB b.jpg\n", argv[0], argv[0]);
    return 1;
  }

  ImageF32 A=load_to_nchw(imgA_path);
  ImageF32 B=load_to_nchw(imgB_path);

  /* ORT init */
  g_api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
  OrtEnv* env=NULL; ort_ok(g_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING,"lpips",&env));
  OrtAllocator* alloc=NULL; ort_ok(g_api->GetAllocatorWithDefaultOptions(&alloc));
  OrtSessionOptions* so=NULL; ort_ok(g_api->CreateSessionOptions(&so));
  /* ort_ok(OrtSessionOptionsAppendExecutionProvider_CoreML(so, 0)); */

  if(!model_feat){
    fprintf(stderr,"Error: --feat lpips_feature.onnx is required for --diff model.\n");
    return 2;
  }
  OrtSession* diff_sess=NULL; ort_ok(g_api->CreateSession(env, model_diff, so, &diff_sess));
  OrtSession* feat_sess=NULL; ort_ok(g_api->CreateSession(env, model_feat, so, &feat_sess));

  // feature 入力サイズに合わせる
  int64_t f_dims[8]={0}; size_t f_rank=0; get_first_input_shape(feat_sess, f_dims, &f_rank);
  int fw=(f_rank>=4 && f_dims[3]>0)?(int)f_dims[3]:A.w;
  int fh=(f_rank>=4 && f_dims[2]>0)?(int)f_dims[2]:A.h;

  float* Afit=(A.w==fw && A.h==fh)? A.nchw : bilinear_resize_nchw3(A.nchw,A.w,A.h,fw,fh);
  float* Bfit=(B.w==fw && B.h==fh)? B.nchw : bilinear_resize_nchw3(B.nchw,B.w,B.h,fw,fh);

  OrtMemoryInfo* mi=NULL; ort_ok(g_api->CreateCpuMemoryInfo(OrtArenaAllocator,OrtMemTypeDefault,&mi));
  int64_t dims[4]={1,3,fh,fw}; size_t bytes=(size_t)1*3*(size_t)fh*(size_t)fw*sizeof(float);
  OrtValue *tA=NULL,*tB=NULL;
  ort_ok(g_api->CreateTensorWithDataAsOrtValue(mi,(void*)Afit,bytes,dims,4,ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,&tA));
  ort_ok(g_api->CreateTensorWithDataAsOrtValue(mi,(void*)Bfit,bytes,dims,4,ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,&tB));

  char* fin0=NULL; ort_ok(g_api->SessionGetInputName(feat_sess,0,alloc,&fin0));
  size_t f_outs=0; ort_ok(g_api->SessionGetOutputCount(feat_sess,&f_outs));
  char** f_out_names=(char**)calloc(f_outs,sizeof(char*));
  for(size_t i=0;i<f_outs;++i) ort_ok(g_api->SessionGetOutputName(feat_sess,i,alloc,&f_out_names[i]));

  const char* f_in_names[1]={fin0};
  const OrtValue* f_in_valsA[1]={tA};
  const OrtValue* f_in_valsB[1]={tB};
  OrtValue** fA=(OrtValue**)calloc(f_outs,sizeof(OrtValue*));
  OrtValue** fB=(OrtValue**)calloc(f_outs,sizeof(OrtValue*));
  ort_ok(g_api->Run(feat_sess,NULL,f_in_names,f_in_valsA,1,(const char* const*)f_out_names,f_outs,fA));
  ort_ok(g_api->Run(feat_sess,NULL,f_in_names,f_in_valsB,1,(const char* const*)f_out_names,f_outs,fB));

  size_t d_ins=0; ort_ok(g_api->SessionGetInputCount(diff_sess,&d_ins));
  char** d_in_names=(char**)calloc(d_ins,sizeof(char*));
  const OrtValue** d_in_vals=(const OrtValue**)calloc(d_ins,sizeof(OrtValue*));
  for(size_t i=0;i<d_ins;++i){
    ort_ok(g_api->SessionGetInputName(diff_sess,i,alloc,&d_in_names[i]));
    int idx=tail_index(d_in_names[i]);
    if(strncmp(d_in_names[i],"feat_x_",7)==0 && idx>=0 && (size_t)idx<f_outs) d_in_vals[i]=fA[idx];
    else if(strncmp(d_in_names[i],"feat_y_",7)==0 && idx>=0 && (size_t)idx<f_outs) d_in_vals[i]=fB[idx];
    else fprintf(stderr,"Warning: unmatched diff input: %s (idx=%d)\n", d_in_names[i], idx);
  }

  char* d_out0=NULL; ort_ok(g_api->SessionGetOutputName(diff_sess,0,alloc,&d_out0));
  const char* d_out_names[1]={d_out0};
  OrtValue* d_out_vals[1]={NULL};
  ort_ok(g_api->Run(diff_sess,NULL,(const char* const*)d_in_names,d_in_vals,d_ins,d_out_names,1,d_out_vals));
  float* out_data=NULL; ort_ok(g_api->GetTensorMutableData(d_out_vals[0],(void**)&out_data));
  printf("LPIPS: %.8f\n", *out_data);

  // cleanup
  g_api->ReleaseValue(d_out_vals[0]);
  ort_ok(g_api->AllocatorFree(alloc,d_out0));
  for(size_t i=0;i<d_ins;++i) ort_ok(g_api->AllocatorFree(alloc,d_in_names[i]));
  free(d_in_names); free((void*)d_in_vals);

  for(size_t i=0;i<f_outs;++i){ g_api->ReleaseValue(fA[i]); g_api->ReleaseValue(fB[i]); }
  free(fA); free(fB);
  for(size_t i=0;i<f_outs;++i) ort_ok(g_api->AllocatorFree(alloc,f_out_names[i]));
  free(f_out_names);
  ort_ok(g_api->AllocatorFree(alloc,fin0));

  g_api->ReleaseValue(tA); g_api->ReleaseValue(tB);
  g_api->ReleaseMemoryInfo(mi);
  if(Afit!=A.nchw) free(Afit);
  if(Bfit!=B.nchw) free(Bfit);
  g_api->ReleaseSession(feat_sess);
  g_api->ReleaseSession(diff_sess);
  

  g_api->ReleaseSessionOptions(so);
  g_api->ReleaseEnv(env);
  free_image(&A); free_image(&B);
  return 0;
}
