#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mini_fs_conf.h"
#include "mini_fs.h"
#include "osd2_0_cmd.h"
#include "bit_display.h"
#include "global_variable.h"
#include "crc16.h"
#include "three_event.h"
#include "gunzip.h"
//#include "reed.h"





static void change_page_into_file(void)
{
    if(gSys_tp.use_page_info_fileid ==  F_PAGE_INFO1)
    {
        gSys_tp.use_page_info_fileid = F_PAGE_INFO2;
    }
    else
    {
        gSys_tp.use_page_info_fileid = F_PAGE_INFO1;
    }
}
static void eraser_page_info_file(void)
{ 

    if(gSys_tp.use_page_info_fileid ==  F_PAGE_INFO1)
    {
        f_erase(F_PAGE_INFO2);
    }
    else
    {
        f_erase(F_PAGE_INFO1);
    }

}

file_id_t no_use_page_num_fileid;
static void change_page_number_file(void)
{
    if(gSys_tp.use_page_num_fileid ==  F_PAGE_NUM_1)
    {
        gSys_tp.use_page_num_fileid = F_PAGE_NUM_2;
        no_use_page_num_fileid = F_PAGE_NUM_1;
    }
    else
    {
        gSys_tp.use_page_num_fileid = F_PAGE_NUM_1;
        no_use_page_num_fileid = F_PAGE_NUM_2;
    }
}
static void eraser_page_number_file(void)
{
    if(gSys_tp.use_page_num_fileid ==  F_PAGE_NUM_1)
    {
        f_erase(F_PAGE_NUM_2);
    }
    else
    {
        f_erase(F_PAGE_NUM_1);
    }
}

bool number_store_fun(file_id_t src_id, UINT16 len,UINT32 offset,UINT8 page_id,UINT8 *src_buf,UINT8 *dst_buf)
{
    UINT16 j,k;
    UINT8 flag=0;

    if(len > PAGE_LEN)
        return FALSE;

    f_read(F_BMP_DATA, offset, (UINT8 *)src_buf,len);//读取相应页的所有数字
    f_read(src_id,(WORD)((page_id) * PAGE_LEN) , (UINT8 *)dst_buf,PAGE_LEN);//读取相应页之前保存在flash中的数据
    k=0;  //此处k=0.，表示每页开始时，从头开始找
    /*
  k=0放在这里的目的，减小查询的机会，出现的风险会是，在找数据的时候出现错误，未找到想要的数据时候，k被加到最大值后，下次在找本页的数据时，越界错误，要加个标志位判断是否找到过此数据
  若没有找到，则需要返回一个错误值，osd，每个命令的错误值需要对应好，目的是保证不必要的擦除动作
     */
    for(j=0;j<len ;j+= sizeof(number_t))//原始数据
    {
        //tp_id=src_buf[j];                          //获取要改变数字对应的显示属性id
        //此处k!=0.，表示每页开始时，从layerid已经排序
        for(;k < PAGE_LEN ; k+= sizeof(number_t))//目的数据
        {
            // if(tp_id == dst_buf[k] )//判断是否存在相同的id
            if((0 == memcmp(src_buf + j, dst_buf + k, sizeof(UINT16))))
            {
                flag = 1;
                memcpy(dst_buf+k,src_buf+j,sizeof(number_t));//存在则替换数值
                break;
            }
        }
        if(flag == 0)
        {
            k=0;
            return FALSE;//在本页中未找到与之相对性的数字
        }
        else
        {
            flag =0;
        }
    }
    f_write_direct(gSys_tp.use_page_num_fileid ,(WORD)((page_id) * PAGE_LEN), (UINT8 *)dst_buf,PAGE_LEN);//本页数字替换完成后，在写入到新的页中


    return TRUE;

}
extern const UINT8 g_page_flag[8];
void stor_number_unchange_fun(UINT8 tp_page_map,file_id_t src_id,UINT8 *dst_temp)
{
    UINT8 i=0;
    for(i=0;i<= ALL_PAGE_SUM;i++)
    {
        if( (tp_page_map & g_page_flag[i]) == 0)//表示此页没被写入过
        {
            f_read(src_id,(WORD)(i * PAGE_LEN) , (UINT8 *)dst_temp,PAGE_LEN);//读取相应页之前保存在flash中的数据
            f_write_direct(gSys_tp.use_page_num_fileid ,(WORD)(i * PAGE_LEN), (UINT8 *)dst_temp,PAGE_LEN);//在写入到新的页中
        }
    }
}

bool save_cmd1_cmd5(UINT8 default_page_id)
{

    if(gchange_page_flag == FALSE)
    {
        Event_post(protocol_eventHandle, EVENT_FALG_DISPLAY_PAGE);
        gpage.flag = FALSE;//停止之前的计数

        gSys_tp.stay_time = 0;
        gSys_tp.stay_time_cont = 0;
        gSys_tp.present_page_id = default_page_id;
    }
    gSys_tp.default_page_id = default_page_id;
    //  if(FALSE == sys_page_display_store_fun(default_page_id,gSys_tp.present_page_id,gSys_tp.gpage_nowid,tp1,tp2))//保存页显示属性
    //    return FALSE;
    return TRUE;
}
static  int32_t cmd1_fun(file_id_t cmdfd, UINT32 offset )
{
    cmd1_t tp;
    file_id_t src_id;
    UINT8 tp_info_src[PAGE_LEN],tp_info_dst[PAGE_LEN];
    UINT8 tp_page_map = 0;//初始化必须有



    src_id = gSys_tp.use_page_num_fileid;
    change_page_number_file();//切换保存数字的文件区
    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(cmd1_t));
    offset += sizeof(cmd1_t);
    tp_page_map |= g_page_flag[tp.default_page_id];
    if(TRUE != number_store_fun(src_id,tp.len-sizeof(tp.default_page_id),offset,tp.default_page_id,tp_info_src,tp_info_dst))
        return -1;

    stor_number_unchange_fun(tp_page_map,src_id,tp_info_dst);//copy为改变的页上的数字
    eraser_page_number_file();

    if(FALSE ==save_cmd1_cmd5(tp.default_page_id))
        return  -1;
#if 1
    gSys_tp.change_map = 0xff;            //全部页设置成已经改变
#else
    gSys_tp.change_map |= tp_page_map;
#endif

    return tp.len + offsetof(cmd1_t,default_page_id);
}

static int32_t cmd2_fun(file_id_t cmdfd, UINT32 offset )
{
#define CHANGE_ALL_NUMBER       (0x01)
#define CHANGE_FORMER_NUMBER    (0x00)


    alter_num_t tp;
    UINT16 i;
    UINT8 tp_info_src[PAGE_LEN],tp_info_dst[PAGE_LEN];
    UINT8 tp_page_map = 0;//初始化必须有
    page_num_sum_t np;
    file_id_t src_id;

    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(alter_num_t));
    offset += sizeof(tp);
    src_id = gSys_tp.use_page_num_fileid;
    change_page_number_file();//切换保存数字的文件区
    if(tp.flag != CHANGE_ALL_NUMBER)//只是修改数据
    {
        for(i=0;i<tp.len - sizeof(tp.flag) ;)//2号命令的总体长度
        {
            f_read(cmdfd, offset, (UINT8 *)&np, sizeof(page_num_sum_t));//读取页号和每个页保存的数字长度
            tp_page_map |= g_page_flag[np.page_id];                    //保存写入过的页

            offset += sizeof(page_num_sum_t);

            if(TRUE != number_store_fun(src_id,np.page_num_len,offset,np.page_id,tp_info_src,tp_info_dst))//修改改变的数字
                return -1;
            offset += np.page_num_len;
            i+= ( sizeof(page_num_sum_t) + np.page_num_len);
            //gSys_tp.change_map |= tp_page_map;//记录那几个页上的数字发生改变
        }

        stor_number_unchange_fun(tp_page_map,src_id,tp_info_dst);//copy未改变的页上的数字
        eraser_page_number_file();
        goto loop;

    }
    //覆盖

    eraser_page_number_file();
    for(i=0;i<tp.len - sizeof(tp.flag) ;)
    {
        f_read(cmdfd, offset, (UINT8 *)&np, sizeof(page_num_sum_t));
        offset += sizeof(page_num_sum_t);
        f_read(cmdfd, offset, (UINT8 *)tp_info_src,np.page_num_len);
        f_write_direct(gSys_tp.use_page_num_fileid ,(WORD)((np.page_id) * PAGE_LEN), (UINT8 *)tp_info_src, np.page_num_len);
        offset += np.page_num_len;
        i+= ( sizeof(page_num_sum_t) + np.page_num_len);
    }
    //gSys_tp.change_map = 0xff;                         //简化逻辑，出现124命令全部认为此页改变，需要重新更新
    loop:
    gSys_tp.change_map = 0xff;
    return (tp.len + 3);//3=cmd+len

}

static int32_t cmd3_fun(file_id_t cmdfd, UINT32 offset )
{ 
#if 0
#define  LEN_NULL (0xffffffff)
#define  FILE_ERR  (0xff)
    write_layer_t tp;
    wrt_layer_arrt_t lay_tp,lay_tp_temp;


    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(write_layer_t));
    lay_tp.offset = offset + offsetof(write_layer_t,layerid);//偏移地址是从layer_id开始
    lay_tp.len = tp.len;
    lay_tp.src_file = cmdfd;
    f_read(F_LAY_MAP, (tp.layerid - 1) * sizeof(wrt_layer_arrt_t), (UINT8 *)&lay_tp_temp, sizeof(wrt_layer_arrt_t));
    if( ((lay_tp_temp.len == LEN_NULL) && (lay_tp_temp.offset == LEN_NULL) && (lay_tp_temp.src_file == FILE_ERR)) || ( (lay_tp_temp.len == lay_tp.len) && (lay_tp_temp.offset == lay_tp.offset) && (lay_tp_temp.src_file == lay_tp.src_file ) ))
    {
        f_write_direct(FILE_LAYER_MAP, (tp.layerid - 1) * sizeof(wrt_layer_arrt_t), (UINT8 *)&lay_tp, sizeof(wrt_layer_arrt_t));//保存3号命令下来的图层属性
    }
    else
    {
        return -1;//相同图层号写入一个地址，报错
    }
#else

    write_layer_t tp;
    wrt_layer_arrt_t lay_tp,lay_tp_temp,cmp_tp;

    memset((UINT8 *)&cmp_tp,0xff,sizeof(wrt_layer_arrt_t));
    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(write_layer_t));
    lay_tp.offset = offset + offsetof(write_layer_t,layerid);//偏移地址是从layer_id开始
    lay_tp.len = tp.len;
    lay_tp.src_file = cmdfd;
    f_read(F_LAY_MAP, (tp.layerid - 1) * sizeof(wrt_layer_arrt_t), (UINT8 *)&lay_tp_temp, sizeof(wrt_layer_arrt_t));
    if((0 == memcmp(&cmp_tp, &lay_tp_temp, sizeof(wrt_layer_arrt_t)))|| (0 == memcmp(&lay_tp, &lay_tp_temp, sizeof(wrt_layer_arrt_t))))
    {
        f_write_direct(FILE_LAYER_MAP, (tp.layerid - 1) * sizeof(wrt_layer_arrt_t), (UINT8 *)&lay_tp, sizeof(wrt_layer_arrt_t));//保存3号命令下来的图层属性
    }
    else
    {
        return -1;//相同图层号写入一个地址，报错
    }
#endif
    return  tp.len + (UINT32)offsetof(write_layer_t,layerid);

}



static int32_t cmd4_fun(file_id_t cmdfd, UINT32 offset )
{


    UINT8 i=0;
    UINT8 tp_info[PAGE_LEN];
    UINT16  num_len;
    store_page_cmd_t tp;
    page_into_t page_tp;

    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(store_page_cmd_t));
    if( (tp.page_sum > ALL_PAGE_SUM+1) )//页数大于7是错误的
        return -1;
    offset += sizeof(store_page_cmd_t);

    change_page_into_file();
    eraser_page_info_file();
    change_page_number_file();
    eraser_page_number_file();
    memset(gSys_tp.page_map,0xff,8);
    gSys_tp.change_map = 0xff;            //全部页设置成已经改变
    for(i = 0 ; i < tp.page_sum ; i++ )
    {
        f_read(cmdfd, offset, (UINT8 *)&page_tp, sizeof(page_into_t));
        memset(tp_info,0xff,PAGE_LEN);//不能删除
        offset += sizeof(page_into_t);//地址加到page_info
        //----非数字图层-----
        if(page_tp.layer_len > PAGE_LEN )
            return -1;
        gSys_tp.page_map[page_tp.page_id] = page_tp.page_id;//保存所有页码，页码下标对应响应的页号

        if(page_tp.layer_len != 0)//非数字图层长度不为0
        {
            f_read(cmdfd, offset, (UINT8 *)&tp_info, page_tp.layer_len);//非数字图层
            f_write_direct(gSys_tp.use_page_info_fileid, (page_tp.page_id) * PAGE_LEN, (UINT8 *)tp_info, page_tp.layer_len );//写入非数字图层
            offset += page_tp.layer_len;  //加上非数字的id长度
        }
        //----数字图层-----
        f_read(cmdfd, offset, (UINT8 *)&num_len, sizeof(num_len));
        offset += sizeof(num_len);//便宜地址加到第一数的layerid处

        if(num_len == 0)//没有数字
            continue;

        if(num_len > PAGE_LEN )
            return -1;
        memset(tp_info,0xff,PAGE_LEN);//能不能删除
        f_read(cmdfd, offset, (UINT8 *)&tp_info, num_len);//读取数字图层的长度
        f_write_direct(gSys_tp.use_page_num_fileid, (page_tp.page_id) * PAGE_LEN, (UINT8 *)tp_info, num_len);//写入非数字图层
        offset += (num_len);//加上数字元素的长度
    }
    gSys_tp.stay_time_cont = 0;
    //  if(FALSE == sys_page_display_store_fun(gSys_tp.default_page_id,gSys_tp.present_page_id,gSys_tp.gpage_nowid,gSys_tp.stay_time,0))//保存页显示属性 默认页、要显示的页、屏幕显示的页、停留时间，已经走过的时间、状态标志
    //    return -1;

    return tp.len + (UINT32)offsetof(store_page_cmd_t,page_sum);

}


static int32_t cmd5_fun(file_id_t cmdfd, UINT32 offset )
{
    dis_page_cmd_t tp;

    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(dis_page_cmd_t));
    if(FALSE ==save_cmd1_cmd5(tp.default_page_id))
        return  -1;
    return sizeof(dis_page_cmd_t);
}

static int32_t cmd7_fun(file_id_t cmdfd, UINT32 offset )
{
#define RC_DATA_START_ADDR 4

    UINT16 len = 0;
    UINT8 temp_buf[32];

    f_read(cmdfd, offset, temp_buf, 30);
    offset += 1;
    memcpy((UINT8 *)&len,temp_buf+1,sizeof(len));
    memcpy((UINT8 *)rc_attr_info.secur_code,temp_buf+1+sizeof(len),20);

    if(rc_attr_info.display_time)
    {
        gSys_tp.present_page_id = rc_attr_info.page_num;
        gSys_tp.stay_time = rc_attr_info.display_time;
        gSys_tp.stay_time_cont = 0;
        //    if(FALSE == sys_page_display_store_fun( gSys_tp.default_page_id,rc_attr_info.page_num,gSys_tp.gpage_nowid ,rc_attr_info.display_time,0))//保存页显示属性 默认页、要显示的页、当前显示的页、停留时间，已经走过的时间、状态标志
        //      return -1;
        gchange_page_flag = TRUE;
        gpage.flag = FALSE;//停止之前的计数
        //    gEventFlag |= EVENT_FALG_DISPLAY_PAGE;
        Event_post(protocol_eventHandle, EVENT_FALG_DISPLAY_PAGE);
    }


    rc_led_init();

    return (1 + len + sizeof(len));

}
static int32_t cmd8_fun(file_id_t cmdfd, UINT32 offset )
{
    UINT16 len = 0,crc= 0;
    UINT8 temp_buf[32];

    f_read(cmdfd, offset, temp_buf, 30);
    offset += 1;
    memcpy((UINT8 *)&len,temp_buf+1,sizeof(len));
    memcpy((UINT8 *)&epd_attr_info,temp_buf+1+sizeof(len),sizeof(epd_attr_info));

    epd_attr_info.global_crc = my_cal_crc16(crc,(UINT8 *)&epd_attr_info,sizeof(epd_attr_info)-sizeof(epd_attr_info.global_crc));
    save_state_info_fun();

    return (1 + len + sizeof(len));
}

static int32_t cmd50_fun(file_id_t cmdfd, UINT32 offset)
{
    UINT8 i=0;
    UINT8 num_buffer[512];
    UINT8 num_buffer_temp[512];
    UINT32 offset_temp;
    lcd_page_num_t  page_tp;
    lcd_store_page_cmd_t tp;

    f_read(cmdfd, offset, (UINT8 *)&tp, sizeof(lcd_store_page_cmd_t));
    if(tp.page_sum > ALL_PAGE_SUM+1)     //页数大于8是错误的
        return -1;
    offset_temp = offset;
    offset += sizeof(lcd_store_page_cmd_t);
    change_page_number_file();

    for(i = 0; i < tp.page_sum; i++)
    {
        f_read(cmdfd, offset, (UINT8 *)&page_tp, sizeof(lcd_page_num_t));     //取出本页数据的page_id 和 page_data_len
        if(page_tp.page_id > ALL_PAGE_SUM+1)     //页数大于8是错误的
            return -1;
        gSys_tp.page_map[page_tp.page_id] = page_tp.page_id;                  //保存所有页码，页码下标对应响应的页号
        if(page_tp.page_data_len == 0)   //无此页数据
        {
            gSys_tp.page_map[page_tp.page_id] = 0xff;
            offset += (page_tp.page_data_len + (UINT16)offsetof(lcd_page_num_t,page_data));
            continue;
        }
        f_read(cmdfd, offset, num_buffer, page_tp.page_data_len + (UINT16)offsetof(lcd_page_num_t,page_data));
        f_read(no_use_page_num_fileid, PAGE_LEN * page_tp.page_id, num_buffer_temp, page_tp.page_data_len + (UINT16)offsetof(lcd_page_num_t,page_data));
        gSys_tp.change_map |= (0x01<<page_tp.page_id);      //记录改变的页码
        if(0 == memcmp(num_buffer, num_buffer_temp, page_tp.page_data_len + (UINT16)offsetof(lcd_page_num_t,page_data)))
        {
            gSys_tp.change_map &= ~(0x01<<page_tp.page_id); //此页数据没有变化,清楚页信息改变的标志
        }
        f_write_direct(gSys_tp.use_page_num_fileid,  PAGE_LEN*page_tp.page_id, num_buffer, page_tp.page_data_len + (UINT16)offsetof(lcd_page_num_t,page_data));
        offset += (page_tp.page_data_len + (UINT16)offsetof(lcd_page_num_t,page_data));
    }
    eraser_page_number_file();
    return (offset - offset_temp);
}

int32_t cmd_err_fun(file_id_t cmdfd, UINT32 offset )
{
    return -1;
}

typedef int32_t (*cmd_cb_t) (file_id_t cmdfd, UINT32 offset);
static const cmd_cb_t cmd_cb[] = {cmd_err_fun,cmd1_fun,cmd2_fun,cmd3_fun,cmd4_fun,cmd5_fun,cmd7_fun,cmd8_fun};

static int32_t cmd_order(file_id_t cmdfd, UINT32 offset)
{
    UINT8 cmd = 0;

    f_read(cmdfd,offset, &cmd, 1);
    if(cmd == 50)
    {
        return cmd50_fun(cmdfd, offset);
    }
    if (cmd == OSD_END_CMD)
    {
        gcmd_tp.len = offset - gcmd_tp.start_addr + 1;//cmd0x76
        f_read(cmdfd, offset+1, (UINT8 *)&g_crc , sizeof(osd_crc_t));
        return 0;
    }
    if((cmd <= OSD_CMD_SUN) && (cmd != 0))
        return cmd_cb[cmd](cmdfd, offset);
    else
        gerr_info = TR3_OSD_CMDERR;
    return -1;
}

extern UINT8 cmd2_buf[];
extern UINT32 cmd_before_offset;
bool process_cmd(file_id_t cmdfd)
{
    UINT32 i = 0;
    int32_t n = 1;
    gcmd_tp.start_addr = cmd_before_offset;
    gflash_empty = 0;
    for (i = 0; n > 0; i += n)
    {
        //	//fprintf(stderr,"[%s]: i = %d,",__FUNCTION__,i);
        n = cmd_order(cmdfd, i + cmd_before_offset);
        //	//fprintf(stderr,"[%s]:cmd size %d\n***************\n",__FUNCTION__, n);
    }

    if(n < 0)
        return FALSE;
    return TRUE;
}

void sid_change_eraflag(void)
{
#ifdef PGK_BIT_FLASH_OPEN
    write_pkg_buff_id  = swp_pkg(write_pkg_buff_id);
    if(f_len(write_pkg_buff_id) != 0 )
        f_erase(write_pkg_buff_id);
#else
    memset(pkg_bit_map,0xff,G_PKG_BIT_MAP_LEN);
#endif
    gsid.old_sid = gsid.now_sid;
    gFlag_bit =1;
    clear_gpkg_fun();//不能删除，原因是osd更新时，sid不同时表示一个新的周期
    memset((UINT8 *)&gosd_pkg,0x00,sizeof(gosd_pkg));//删除osd数据包统计函数
    cmd_start_offset = f_len(F_BMP_DATA);
    cmd_before_offset = cmd_start_offset;

    first_lose_pkg = 0;
}

void osd_init(void)
{
    write_temp_buff_id = F_BMP_DATA;
#ifdef PGK_BIT_FLASH_OPEN
    write_pkg_buff_id = F_BMP_PKG_1;
    f_erase(F_BMP_PKG_1);
    f_erase(F_BMP_PKG_2);
#endif
    gerr_info = NONEERR;
    memset((UINT8 *)&g_crc,0x00,sizeof(g_crc));
    memset((UINT8 *)&gosd_pkg,0x00,sizeof(gosd_pkg));//删除osd数据包统计函数
}

void event_128_fun(void)
{
    switch (eraser_file_flag)
    {
    case 2:
        f_erase(F_PAGE_INFO1);
        f_erase(F_PAGE_INFO2);
    case 4:
        f_erase(F_PAGE_NUM_1);
        f_erase(F_PAGE_NUM_2);
        break;
    default:
        fs_erase_all();
        f_init_check(1);
        f_init();
        gflash_empty = 0;//放在这里清的原因是，自由整片擦除才起作用，局部擦除不管用
        break;
    }
    gerr_info = NONEERR;
    gchange_page_flag = FALSE;
    memset(gSys_tp.page_map,0xff,8);
    save_sys_load_page_info();
    clear_gpkg_fun();
    gsid.now_sid = 0;
    gsid.old_sid = 0;
}

void erase_file_fun(void)
{
}

RF_CMD_T get_osd_76cmd_fun(UINT8 *buf)
{

    ret_ack_flag = RF_EVENT_OSD_76CMD;
    // gpkg.pkg_num  = 1;
    UINT8 temp[8]={0};

    if(0 == memcmp(&g_crc, &temp,sizeof(g_crc)))
    {
        gerr_info = TR3_OSD_QUERY_ERR;   //判断是否重启，如果重启g_crc =0
    }
    else
    {
        Event_post(protocol_eventHandle, EVENT_FLAG_UPDATA_CHECK);
    }
    return RF_FSM_CMD_RX_DATA;
}

