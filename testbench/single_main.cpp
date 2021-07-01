#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <fstream>

#include <string>
#include "../src_gold/wino_gold.h"
#include "../software/buffer.h"

#include "debug.hpp"
#include "../src/wino.h"
#include "../src/wino_struct.h"
#include "../src/wino_macro.h"
#include "../src/misc.h"
#include "../software/param.h"
#include "../software/softlayer.hpp"
#include "../software/exec.h"
#include <ap_int.h>


#include <cstdint>


int main(int argc, char** argv)
{



    // cxxopts::Options options("MyProgram", "One line description of MyProgram");
    // options.add_options()
    // ("ih", "input height", cxxopts::value<int>())
    // ("iw", "input width", cxxopts::value<int>())
    // ("id", "input depth", cxxopts::value<int>())
    // ("oh", "output height", cxxopts::value<int>())
    // ("ow", "output width", cxxopts::value<int>())
    // ("od", "output depth", cxxopts::value<int>())
    // ("ks", "kernel size", cxxopts::value<int>())
    // ("ss", "stride size", cxxopts::value<int>())
    // ("ps", "pad size", cxxopts::value<int>())
    // ("relu", "relu flag", cxxopts::value<int>())
    // ("scale", "output write back scale factor", cxxopts::value<int>())
    // ("ii", "input initialization pattern", cxxopts::value<std::string>())
    // ("wi", "weight initialization pattern", cxxopts::value<std::string>())
    // ("desc","conv description generation name", cxxopts::value<std::string>())
    // ("og", "golden output file name",cxxopts::value<std::string>()) ;

    // auto option_dict = options.parse(argc, argv);


    // load input arguments
    int input_height = atoi(argv[1]);
    int input_width = atoi(argv[2]);
    int input_depth = atoi(argv[3]);
    int output_height = atoi(argv[4]);
    int output_width = atoi(argv[5]);
    int output_depth = atoi(argv[6]);
    int kernel_size_h = atoi(argv[7]);
    int kernel_size_w = atoi(argv[8]);
    int stride_size = atoi(argv[9]);
    int pad_size_h = atoi(argv[10]);
    int pad_size_w = atoi(argv[11]);
    int relu_flag = atoi(argv[12]);
    int Scale_oback_int = atoi(argv[13]);
    std::string input_gold(argv[14]);
    std::string output_gold(argv[15]);
    std::string weight_gold(argv[16]);
    std::string dump_method(argv[17]);
    std::string folder(argv[18]);
    int use_kernel_size = atoi(argv[19]);
    int group_indepth_offset=0;
    int group_indepth=ALIGN(input_depth,8);
    int group_outdepth_offset=0;
    int group_outdepth=ALIGN(output_depth,8);



    // set layer info

    LayerInfo_t layer_info;
    layer_info.layer_name="conv";
    layer_info.layer_type="conv";
    layer_info.indim[0]=input_depth;
    layer_info.indim[1]=input_height;
    layer_info.indim[2]=input_width;
    layer_info.outdim[0]=output_depth;
    layer_info.outdim[1]=output_height;
    layer_info.outdim[2]=output_width;
    layer_info.kernel_size_h=kernel_size_h;
    layer_info.kernel_size_w=kernel_size_w;
    layer_info.stride=stride_size;
    layer_info.pad_size_h=pad_size_h;
    layer_info.pad_size_w=pad_size_w;
    layer_info.group_indepth=group_indepth;
    layer_info.group_indepth_offset=group_indepth_offset;
    layer_info.group_outdepth=group_outdepth;
    layer_info.group_outdepth_offset=group_outdepth_offset;
    layer_info.relu_flag=relu_flag;
    layer_info.Scale_oback_int=Scale_oback_int;
    layer_info.use_kernel_size=use_kernel_size;
    layer_info.bottoms.push_back("in");
    layer_info.tops.push_back("out");


    // instantiate a layer info vector for use
    std::vector<LayerInfo_t > linfo_vect;
    linfo_vect.push_back(layer_info);


    // declare layer info wiehgt
    char* weight_int = new char[65535*16*4];
    short* bias_int = new short[1024*4 ];
    
    linfo_vect[0].weightbuffer_quant.push_back(weight_int);
    linfo_vect[0].biasbuffer_quant.push_back(bias_int);

    float* weight_float = new float[ kernel_size_h*kernel_size_w* input_depth*output_depth];
    float* bias_float = new float[ output_depth ];
    
    linfo_vect[0].weightbuffers_sw.push_back(weight_float);
    linfo_vect[0].biasbuffers_sw.push_back(bias_float);

    prepare_conv_descriptor(linfo_vect);


    int* conv_desc_param=new int[65535];
    int* bias=new int[1024];
    memcpy(conv_desc_param,&(linfo_vect[0].conv_desc),sizeof(ConvDesc_t));

    FILE* fptr=fopen("param.bin","w");
    fwrite(conv_desc_param,sizeof(int),128,fptr);
    fclose(fptr);


    for(int i=0;i<128;i++)
    {
        std::cout<<"param["<<i<<"]:"<<conv_desc_param[i]<<std::endl;
    }


    if(dump_method=="dump_param")
    {
        delete[] weight_int;
        delete[] bias_int;
        delete[] weight_float;
        delete[] bias_float;
        delete[] conv_desc_param;
        delete[] bias;
        return 0;
    }



    alloc_hw_weight_buffer(linfo_vect);



 

    BlobInfo_t in_blob, out_blob;

    in_blob.blob_name="in";
    in_blob.dim[0]=input_depth;
    in_blob.dim[1]=input_height;
    in_blob.dim[2]=input_width;
    
    out_blob.blob_name="out";
    out_blob.dim[0]=output_depth;
    out_blob.dim[1]=output_height;
    out_blob.dim[2]=output_width;

    std::vector<BlobInfo_t > blobinfo_vect;
    blobinfo_vect.push_back(in_blob);
    blobinfo_vect.push_back(out_blob);


    std::map<std::string,FeatureMapBuffer_t> fmap_dict;
    initiate_featuremap_dict(fmap_dict,blobinfo_vect);
    alloc_float_featuremap_mem(fmap_dict,blobinfo_vect);    
    alloc_int_featuremap_mem(fmap_dict,blobinfo_vect);
    alloc_hw_featuremap_mem_single_blob(fmap_dict["in"]);
    alloc_hw_featuremap_mem_single_blob(fmap_dict["out"]);


    feature_map_init(fmap_dict["in"],input_gold);
 

    featuremap_int_to_hw_pointers(
        fmap_dict["in"].buffers_int[0],
        fmap_dict["in"].buffers_int[1],
        fmap_dict["in"].buffers_hw[0],
        fmap_dict["in"].blob_info->dim[1],
        fmap_dict["in"].blob_info->dim[2],
        fmap_dict["in"].blob_info->dim[0],
        0,
        ALIGN(fmap_dict["in"].blob_info->dim[0],8)
    );
  


    init_weight<char>(linfo_vect[0].weightbuffer_quant[0],input_depth,output_depth,kernel_size_h,kernel_size_w,weight_gold);

    process_hw_weight_buffer_single_layer(linfo_vect[0]);
    // print_weight<0>( (ap_uint<128>*) linfo_vect[0].weightbuffers_hw[0],"idepthweight.txt", linfo_vect[0].port_segment_size[0]*WEIGHT_PORT_NUM*128/8);


    init_bias_int(linfo_vect[0].biasbuffer_quant[0],output_depth,"zero");
if(dump_method!="dump_param" )
{
//    wino_model_int(fmap_dict["in"].buffers_int[0],
//        input_depth,
//        input_height,
//        input_width,
//        fmap_dict["out"].buffers_int[0],
//        output_depth,
//        output_height,
//        output_width,
//        linfo_vect[0].weightbuffer_quant[0],
//        linfo_vect[0].biasbuffer_quant[0],
//        kernel_size_h,
//        kernel_size_w,
//        pad_size_h,
//        pad_size_w,
//        stride_size,
//        relu_flag,
//        Scale_oback_int,
//        use_kernel_size
//    );
//
//    wino_model_int(fmap_dict["in"].buffers_int[1],
//        input_depth,
//        input_height,
//        input_width,
//        fmap_dict["out"].buffers_int[1],
//        output_depth,
//        output_height,
//        output_width,
//        linfo_vect[0].weightbuffer_quant[0],
//        linfo_vect[0].biasbuffer_quant[0],
//        kernel_size_h,
//        kernel_size_w,
//        pad_size_h,
//        pad_size_w,
//        stride_size,
//        relu_flag,
//        Scale_oback_int,
//        use_kernel_size
//    );
}
    char* inputddr=fmap_dict["in"].buffers_hw[0];
    char* outputddr=fmap_dict["out"].buffers_hw[0];
    char* weightddr0=linfo_vect[0].weightbuffers_hw[0];
    char* weightddr1=linfo_vect[0].weightbuffers_hw[1];
    char* weightddr2=linfo_vect[0].weightbuffers_hw[2];
    char* weightddr3=linfo_vect[0].weightbuffers_hw[3];

    if(dump_method=="dump_txt")
    {
        print_feature_map<char>(fmap_dict["in"].buffers_int[0],
        "C1_mdoel_int_in.txt",
        linfo_vect[0].indim[1],
        linfo_vect[0].indim[2],
        linfo_vect[0].indim[0]
        );
        
        print_feature_map<char>(fmap_dict["in"].buffers_int[1],
        "C2_mdoel_int_in.txt",
        linfo_vect[0].indim[1],
        linfo_vect[0].indim[2],
        linfo_vect[0].indim[0]
        );
    }

    




    fptr=fopen("weight.bin","w");

    fwrite(linfo_vect[0].weightbuffers_hw[0],1,linfo_vect[0].weightbuffers_hw_size[0],fptr);
    fclose(fptr);

    fptr=fopen("input.bin","w");

    fwrite(fmap_dict["in"].buffers_hw[0], 16,CEIL_DIV(input_depth,8)*input_height*ALIGN(input_height,8),fptr);
    fclose(fptr);

    if(dump_method == "compare" || dump_method=="dump_txt")
    {
        wino_systolic_top(
            (ap_uint<128> *) inputddr,
            (ap_uint<128> *) inputddr,
            // (ap_uint<128> *) inputddr,
            // (ap_uint<128> *) inputddr,
            WEIGHT_PORTS_CALL((ap_uint<128>*) weightddr),
            (ap_uint<ODDR_WIDTH*BATCH_SIZE*OUT_PORT_BATCH_NUM> *) outputddr,
            (ap_uint<ODDR_WIDTH*BATCH_SIZE*OUT_PORT_BATCH_NUM> *) outputddr,
            // (ap_uint<ODDR_WIDTH*BATCH_SIZE*OUT_PORT_BATCH_NUM> *) outputddr,
            // (ap_uint<ODDR_WIDTH*BATCH_SIZE*OUT_PORT_BATCH_NUM> *) outputddr,
            (ap_int<32>*) & (linfo_vect[0].conv_desc)
            // (ap_int<32>*) bias
            // (linfo_vect[0].biasbuffer_quant[0])
            );
    }
    else if(dump_method== "dump_gen"   )
    {
        char filename[100];
        sprintf(filename, "bin/cnm_%d_%d_%d_%d_output.bin", output_height,output_depth,kernel_size_h,kernel_size_w);
        if(  (fptr=fopen(filename,"rb")) !=NULL)
        {
            fread(fmap_dict["out"].buffers_hw[0], 16,output_depth/8*output_height*output_height,fptr);
            fclose(fptr);
        }
        else
            std::cout<<"output not found!"<<std::endl;
    }
    


 
    delete [] conv_desc_param;
    delete [] bias;

    if(dump_method=="dump_bin")
    {
        featuremap_int_to_hw_pointers(
        fmap_dict["out"].buffers_int[0],
        fmap_dict["out"].buffers_int[1],
        fmap_dict["out"].buffers_hw[0],
        fmap_dict["out"].blob_info->dim[1],
        fmap_dict["out"].blob_info->dim[2],
        fmap_dict["out"].blob_info->dim[0],
        0,
        ALIGN(fmap_dict["out"].blob_info->dim[0],8));



        fptr=fopen("output.bin","w");
        fwrite(fmap_dict["out"].buffers_hw[0], 16, CEIL_DIV(output_depth,8)*output_height*ALIGN(output_height,8),fptr);
        fclose(fptr);
        del_featuremap_mem(fmap_dict);
        del_weight_buffer_pointer(linfo_vect);
        return 0;
    }
    else if( dump_method=="dump_txt" || dump_method == "dump_gen" )
    {
        // char* out_hw1 = new char[ fmap_dict["out"].buffer_size_int];
        // char* out_hw2 = new char[ fmap_dict["out"].buffer_size_int];

        // featuremap_hw_to_int_pointers(
        //     fmap_dict["out"].buffers_hw[0],
        //     out_hw1,
        //     out_hw2,
        //     linfo_vect[0].outdim[1],
        //     linfo_vect[0].outdim[2],
        //     linfo_vect[0].outdim[0],
        //     0,
        //     linfo_vect[0].outdim[0]
        // );



        char filename[100];
        sprintf(filename, "%s/hw_%d_%d_%d_%d.txt",folder.c_str(),input_height,input_depth,kernel_size_h,kernel_size_w);

    
        print_hw_buffers<char>(
            fmap_dict["out"].buffers_hw[0],
            filename,
            linfo_vect[0].outdim[1],
            linfo_vect[0].outdim[2],
            linfo_vect[0].outdim[0]);


        featuremap_int_to_hw_pointers(
        fmap_dict["out"].buffers_int[0],
        fmap_dict["out"].buffers_int[1],
        fmap_dict["out"].buffers_hw[0],
        fmap_dict["out"].blob_info->dim[1],
        fmap_dict["out"].blob_info->dim[2],
        fmap_dict["out"].blob_info->dim[0],
        0,
        ALIGN(fmap_dict["out"].blob_info->dim[0],8));


        sprintf(filename, "%s/model_%d_%d_%d_%d.txt",folder.c_str(),input_height,input_depth,kernel_size_h,kernel_size_w);

    
        print_hw_buffers<char>(
            fmap_dict["out"].buffers_hw[0],
            filename,
            linfo_vect[0].outdim[1],
            linfo_vect[0].outdim[2],
            linfo_vect[0].outdim[0]);



        del_featuremap_mem(fmap_dict);
        del_weight_buffer_pointer(linfo_vect);
        
        return 0;
        
        
    }
    else
    {
        char* out_hw1 = new char[ fmap_dict["out"].buffer_size_int];
        char* out_hw2 = new char[ fmap_dict["out"].buffer_size_int];

        featuremap_hw_to_int_pointers(
            fmap_dict["out"].buffers_hw[0],
            out_hw1,
            out_hw2,
            linfo_vect[0].outdim[1],
            linfo_vect[0].outdim[2],
            linfo_vect[0].outdim[0],
            0,
            linfo_vect[0].outdim[0]
        );

        bool yes1=diff_feature_map<char>(fmap_dict["out"].buffers_int[0], out_hw1,linfo_vect[0].outdim[1],linfo_vect[0].outdim[2],linfo_vect[0].outdim[0]);
        bool yes2=diff_feature_map<char>(fmap_dict["out"].buffers_int[1], out_hw2,linfo_vect[0].outdim[1],linfo_vect[0].outdim[2],linfo_vect[0].outdim[0]);

        delete [] out_hw1;
        delete [] out_hw2;





        del_featuremap_mem(fmap_dict);
        del_weight_buffer_pointer(linfo_vect);


        if(yes1 && yes2){
            printf("Not different!\n");
        } 
        else return 3;
    }
  

    // FREE(input_DDR);
    // FREE(weight_DDR);
    // FREE(mem_param);
}


