/*  Copyright (c) BAVC. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Core/VideoStats.h"
#include "Core/VideoCore.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern "C"
{
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <libavutil/frame.h>
}

#include "tinyxml2.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cfloat>
using namespace tinyxml2;
//---------------------------------------------------------------------------

//***************************************************************************
// Constructor / Destructor
//***************************************************************************

//---------------------------------------------------------------------------
VideoStats::VideoStats (size_t FrameCount, double Duration, double Frequency_)
    :
    CommonStats(VideoPerItem, 0, Group_VideoMax, Item_VideoMax, FrameCount, Duration, Frequency_)
{
}

//---------------------------------------------------------------------------
VideoStats::~VideoStats()
{
}

//***************************************************************************
// External data
//***************************************************************************

//---------------------------------------------------------------------------
void VideoStats::StatsFromExternalData (const string &Data)
{
    // VideoStats from external data
    // XML input
    XMLDocument Document;
    if (Document.Parse(Data.c_str()))
       return;

    XMLElement* Root=Document.FirstChildElement("ffprobe:ffprobe");
    if (Root)
    {
        XMLElement* Frames=Root->FirstChildElement("frames");
        if (Frames)
        {
            XMLElement* Frame=Frames->FirstChildElement();
            while (Frame)
            {
                if (!strcmp(Frame->Value(), "frame"))
                {
                    const char* media_type=Frame->Attribute("media_type");
                    if (media_type && !strcmp(media_type, "video"))
                    {
                        if (x_Current>=Data_Reserved)
                            Data_Reserve(x_Current);

                        const char* Attribute;
                            
                        x[0][x_Current]=x_Current;

                        Attribute=Frame->Attribute("pkt_duration_time");
                        if (Attribute)
                            durations[x_Current]=std::atof(Attribute);

                        Attribute=Frame->Attribute("key_frame");
                        if (Attribute)
                            key_frames[x_Current]=std::atof(Attribute)?true:false;

                        Attribute=Frame->Attribute("pkt_pts_time");
                        if (!Attribute || !strcmp(Attribute, "N/A"))
                            Attribute=Frame->Attribute("pkt_dts_time");
                        if (Attribute && strcmp(Attribute, "N/A"))
                        {
                            x[1][x_Current]=std::atof(Attribute);
                            if (FirstTimeStamp==DBL_MAX)
                                FirstTimeStamp=x[1][x_Current];
                            if (x[1][x_Current]<FirstTimeStamp)
                            {
                                double Difference=FirstTimeStamp-x[1][x_Current];
                                for (size_t Pos=0; Pos<x_Current; Pos++)
                                {
                                    x[1][Pos]-=Difference;
                                    x[2][Pos]=x[1][Pos]/60;
                                    x[3][Pos]=x[2][Pos]/60;
                                }
                            }
                            x[1][x_Current]-=FirstTimeStamp;
                            x[2][x_Current]=x[1][x_Current]/60;
                            x[3][x_Current]=x[2][x_Current]/60;
                        }

                        int Width;
                        Attribute=Frame->Attribute("width");
                        if (Attribute)
                            Width=std::atoi(Attribute);
                        else
                            Width=0;

                        int Height;
                        Attribute=Frame->Attribute("width");
                        if (Attribute)
                            Height=std::atoi(Attribute);
                        else
                            Height=0;

                        XMLElement* Tag=Frame->FirstChildElement();
                        while (Tag)
                        {
                            if (!strcmp(Tag->Value(), "tag"))
                            {
                                size_t j=Item_VideoMax;
                                const char* key=Tag->Attribute("key");
                                if (key)
                                    for (size_t Plot_Pos=0; Plot_Pos<Item_VideoMax; Plot_Pos++)
                                        if (!strcmp(key, PerItem[Plot_Pos].FFmpeg_Name))
                                        {
                                            j=Plot_Pos;
                                            break;
                                        }

                                if (j!=Item_VideoMax)
                                {
                                    double value;
                                    Attribute=Tag->Attribute("value");
                                    if (Attribute)
                                        value=std::atof(Attribute);
                                    else
                                        value=0;
                                            
                                    // Special cases: crop: x2, y2
                                    if (Width && !strcmp(key, "lavfi.cropdetect.x2"))
                                        y[j][x_Current]=Width-value;
                                    else if (Height && !strcmp(key, "lavfi.cropdetect.y2"))
                                        y[j][x_Current]=Height-value;
                                    else if (Width && !strcmp(key, "lavfi.cropdetect.w"))
                                        y[j][x_Current]=Width-value;
                                    else if (Height && !strcmp(key, "lavfi.cropdetect.h"))
                                        y[j][x_Current]=Height-value;
                                    else
                                        y[j][x_Current]=value;

                                    if (PerItem[j].Group1!=Group_VideoMax && y_Max[PerItem[j].Group1]<y[j][x_Current])
                                        y_Max[PerItem[j].Group1]=y[j][x_Current];
                                    if (PerItem[j].Group2!=Group_VideoMax && y_Max[PerItem[j].Group2]<y[j][x_Current])
                                        y_Max[PerItem[j].Group2]=y[j][x_Current];
                                    if (PerItem[j].Group1!=Group_VideoMax && y_Min[PerItem[j].Group1]>y[j][x_Current])
                                        y_Min[PerItem[j].Group1]=y[j][x_Current];
                                    if (PerItem[j].Group2!=Group_VideoMax && y_Min[PerItem[j].Group2]>y[j][x_Current])
                                        y_Min[PerItem[j].Group2]=y[j][x_Current];

                                    //VideoStats
                                    Stats_Totals[j]+=y[j][x_Current];
                                    if (PerItem[j].DefaultLimit!=DBL_MAX)
                                    {
                                        if (y[j][x_Current]>PerItem[j].DefaultLimit)
                                            Stats_Counts[j]++;
                                        if (PerItem[j].DefaultLimit2!=DBL_MAX && y[j][x_Current]>PerItem[j].DefaultLimit2)
                                            Stats_Counts2[j]++;
                                    }
                                }
                            }

                            Tag=Tag->NextSiblingElement();
                        }




                        if (x_Max[0]<=x[0][x_Current])
                        {
                            x_Max[0]=x[0][x_Current];
                            x_Max[1]=x[1][x_Current];
                            x_Max[2]=x[2][x_Current];
                            x_Max[3]=x[3][x_Current];
                        }
                        x_Current++;
                        if (x_Current_Max<=x_Current)
                            x_Current_Max=x_Current;
                    }
                }

                Frame=Frame->NextSiblingElement();
            }
        }
    }

    Frequency=1;
    StatsFinish();
}

//---------------------------------------------------------------------------
void VideoStats::StatsFromFrame (struct AVFrame* Frame, int Width, int Height)
{
    AVDictionary * m=av_frame_get_metadata (Frame);
    AVDictionaryEntry* e=NULL;
    string A;
    for (;;)
    {
        e=av_dict_get     (m, "", e, AV_DICT_IGNORE_SUFFIX);
        if (!e)
            break;
        size_t j=0;
        for (; j<Item_VideoMax; j++)
        {
            if (strcmp(e->key, PerItem[j].FFmpeg_Name)==0)
                break;
        }

        if (j<Item_VideoMax)
        {
            double value=std::atof(e->value);
                                            
            // Special cases: crop: x2, y2
            if (string(e->key)=="lavfi.cropdetect.x2")
                y[j][x_Current]=Width-value;
            else if (string(e->key)=="lavfi.cropdetect.y2")
                y[j][x_Current]=Height-value;
            else if (string(e->key)=="lavfi.cropdetect.w")
                y[j][x_Current]=Width-value;
            else if (string(e->key)=="lavfi.cropdetect.h")
                y[j][x_Current]=Height-value;
            else
                y[j][x_Current]=value;

            if (PerItem[j].Group1!=Group_VideoMax && y_Max[PerItem[j].Group1]<y[j][x_Current])
                y_Max[PerItem[j].Group1]=y[j][x_Current];
            if (PerItem[j].Group2!=Group_VideoMax && y_Max[PerItem[j].Group2]<y[j][x_Current])
                y_Max[PerItem[j].Group2]=y[j][x_Current];
            if (PerItem[j].Group1!=Group_VideoMax && y_Min[PerItem[j].Group1]>y[j][x_Current])
                y_Min[PerItem[j].Group1]=y[j][x_Current];
            if (PerItem[j].Group2!=Group_VideoMax && y_Min[PerItem[j].Group2]>y[j][x_Current])
                y_Min[PerItem[j].Group2]=y[j][x_Current];

            //Stats
            Stats_Totals[j]+=y[j][x_Current];
            if (PerItem[j].DefaultLimit!=DBL_MAX)
            {
                if (y[j][x_Current]>PerItem[j].DefaultLimit)
                    Stats_Counts[j]++;
                if (PerItem[j].DefaultLimit2!=DBL_MAX && y[j][x_Current]>PerItem[j].DefaultLimit2)
                    Stats_Counts2[j]++;
            }
        }

        /*
        A+=e->key;
        A+=',';
        A+=e->value;
        A+="\r\n";
        */
    }

    key_frames[x_Current]=Frame->key_frame?true:false;

    if (x_Max[0]<=x[0][x_Current])
    {
        x_Max[0]=x[0][x_Current];
        x_Max[1]=x[1][x_Current];
        x_Max[2]=x[2][x_Current];
        x_Max[3]=x[3][x_Current];
    }
    x_Current++;
    if (x_Current_Max<=x_Current)
        x_Current_Max=x_Current;
}

//---------------------------------------------------------------------------
void VideoStats::TimeStampFromFrame (struct AVFrame* Frame, size_t FramePos)
{
    if (Frequency==0)
        return; // Not supported

    if (FramePos>=x_Current_Max)
    {
        x_Current_Max=FramePos+1;
        if (x_Current_Max>Data_Reserved)
            Data_Reserve(x_Current_Max);
    }

    x[0][FramePos]=FramePos;

    int64_t ts=(Frame->pkt_pts==AV_NOPTS_VALUE)?Frame->pkt_dts:Frame->pkt_pts; // Using DTS is PTS is not available
    if (ts==AV_NOPTS_VALUE && FramePos)
        ts=(int64_t)((FirstTimeStamp+x[1][FramePos-1]+durations[FramePos-1])*Frequency); // If time stamp is not present, creating a fake one from last frame duration
    if (ts!=AV_NOPTS_VALUE)
    {
        if (FirstTimeStamp==DBL_MAX)
            FirstTimeStamp=ts/Frequency;
        x[1][FramePos]=((double)ts)/Frequency;
        if (x[1][FramePos]<FirstTimeStamp)
        {
            double Difference=FirstTimeStamp-x[1][x_Current];
            for (size_t Pos=0; Pos<x_Current; Pos++)
            {
                x[1][Pos]-=Difference;
                x[2][Pos]=x[1][Pos]/60;
                x[3][Pos]=x[2][Pos]/60;
            }
        }
        x[1][FramePos]-=FirstTimeStamp;
        x[2][FramePos]=x[1][FramePos]/60;
        x[3][FramePos]=x[2][FramePos]/60;
    }
    if (Frame->pkt_duration!=AV_NOPTS_VALUE)
        durations[FramePos]=((double)Frame->pkt_duration)/Frequency;
}

//---------------------------------------------------------------------------
string VideoStats::StatsToCSV()
{
    stringstream Value;
    Value<<",,,,,,pts,,,,,,,,,,,,,,,YMIN,YLOW,YAVG,YHIGH,YMAX,UMIN,ULOW,UAVG,UHIGH,UMAX,VMIN,VLOW,VAVG,VHIGH,VMAX,YDIF,UDIF,VDIF,SATMIN,SATLOW,SATAVG,SATHIGH,SATMAX,HUEMED,HUEAVG,TOUT,VREP,BRNG,CROPx1,CROPx2,CROPy1,CROPy2,CROPw,CROPh,MSEy,MSEu,MSEv,PSNRy,PSNRu,PSNRv";
    #ifdef _WIN32
        Value<<"\r\n";
    #else
        #if defined(__APPLE__) && defined(__MACH__)
            Value<<"\r";
        #else
            Value<<"\n";
        #endif
    #endif
    for (size_t Pos=0; Pos<x_Current; Pos++)
    {
        Value<<",,,,,,";
        Value<<fixed<<setprecision(6)<<x[1][Pos];
        Value<<",,,,,,,,,,,,,,";
        for (size_t Pos2=0; Pos2<Item_VideoMax; Pos2++)
        {
            Value<<','<<fixed<<setprecision(PerItem[Pos2].DigitsAfterComma)<<y[Pos2][Pos];
        }
        #ifdef _WIN32
            Value<<"\r\n";
        #else
            #if defined(__APPLE__) && defined(__MACH__)
                Value<<"\r";
            #else
                Value<<"\n";
            #endif
        #endif
    }

    return Value.str();
}

//---------------------------------------------------------------------------
string VideoStats::StatsToXML (int Width, int Height)
{
    stringstream Data;

    // Per frame (note: the XML header and footer are not created here)
    stringstream width; width<<Width; // Note: we use the same value for all frame, we should later use the right value per frame
    stringstream height; height<<Height; // Note: we use the same value for all frame, we should later use the right value per frame
    for (size_t x_Pos=0; x_Pos<x_Current; ++x_Pos)
    {
        stringstream pkt_pts_time; pkt_pts_time<<fixed<<setprecision(7)<<(x[1][x_Pos]+FirstTimeStamp);
        stringstream pkt_duration_time; pkt_duration_time<<fixed<<setprecision(7)<<durations[x_Pos];
        stringstream key_frame; key_frame<<key_frames[x_Pos]?'1':'0';
        Data<<"        <frame media_type=\"video\" key_frame=\"" << key_frame.str() << "\" pkt_pts_time=\"" << pkt_pts_time.str() << "\"";
        if (pkt_duration_time)
            Data<<" pkt_duration_time=\"" << pkt_duration_time.str() << "\"";
        Data<<" width=\"" << width.str() << "\" height=\"" << height.str() <<"\">\n";

        for (size_t Plot_Pos=0; Plot_Pos<Item_VideoMax; Plot_Pos++)
        {
            string key=PerItem[Plot_Pos].FFmpeg_Name;

            stringstream value;
            switch (Plot_Pos)
            {
                case Item_Crop_x2 :
                case Item_Crop_w :
                                        // Special case, values are from width
                                        value<<Width-y[Plot_Pos][x_Pos];
                                        break;
                case Item_Crop_y2 :
                case Item_Crop_h :
                                        // Special case, values are from height
                                        value<<Height-y[Plot_Pos][x_Pos];
                                        break;
                default:
                                        value<<y[Plot_Pos][x_Pos];
            }

            Data<<"            <tag key=\""+key+"\" value=\""+value.str()+"\"/>\n";
        }

        Data<<"        </frame>\n";
    }

   return Data.str();
}
