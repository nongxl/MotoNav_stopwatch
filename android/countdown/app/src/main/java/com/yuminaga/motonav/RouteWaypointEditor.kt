package com.yuminaga.motonav

import android.text.Editable
import android.text.TextWatcher
import android.view.Gravity
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import android.content.Context
import com.amap.api.services.core.LatLonPoint
import android.view.inputmethod.EditorInfo
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.widget.TextViewCompat
import com.amap.api.services.help.Inputtips
import com.amap.api.services.help.InputtipsQuery
import com.amap.api.services.help.Tip

/** Shared editor for web-motorcycle and native driving route constraints. */
object RouteWaypointEditor {
    const val MAX=3
    fun summary(points:List<MotorPoint>):String=if(points.isEmpty())"添加途经点（最多 $MAX 个）"
        else "途经 ${points.size} 个地点 · 点击编辑"
    fun show(a:AppCompatActivity,points:List<MotorPoint>,near:MotorPoint?=null,apply:(List<MotorPoint>)->Unit){
        val s=NavStyle;fun dp(n:Int)=s.dpi(a,n)
        fun text(value:String,size:Float=14f,color:Int=s.TEXT)=TextView(a).apply{
            text=value;textSize=size;setTextColor(color);typeface=s.MEDIUM;includeFontPadding=false
        }
        fun button(value:String,action:()->Unit)=text(value,13f,s.CYAN).apply{
            gravity=Gravity.CENTER;background=s.round(a,s.CARD2,10f);setOnClickListener{action()}
        }
        val draft=points.toMutableList()
        val box=LinearLayout(a).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(18),dp(10),dp(18),0)}
        val note=text("按列表顺序经过，最多 $MAX 个地点",12f,s.SUB);box.addView(note)
        val rows=LinearLayout(a).apply{orientation=LinearLayout.VERTICAL}
        box.addView(ScrollView(a).apply{addView(rows)},LinearLayout.LayoutParams(-1,dp(190)))
        lateinit var render:()->Unit
        val edit:(Int)->Unit={index->pick(a,"选择途经点 ${index+1}",near){point->
            if(index in draft.indices)draft[index]=point
            else if(index==draft.size&&draft.size<MAX)draft.add(point)
            render()
        }}
        val add=button("＋ 添加途经点"){edit(draft.size)}.apply{contentDescription="添加途经点"}
        box.addView(add,LinearLayout.LayoutParams(-1,dp(42)).apply{topMargin=dp(8)})
        render={
            rows.removeAllViews()
            if(draft.isEmpty())rows.addView(text("尚未添加途经点",14f,s.SUB).apply{setPadding(0,dp(20),0,0)})
            draft.forEachIndexed{index,point->
                val row=LinearLayout(a).apply{gravity=Gravity.CENTER_VERTICAL}
                row.addView(text("${index+1}. ${point.name.ifBlank{"已选地点"}}",14f).apply{
                    maxLines=2;setPadding(0,dp(8),dp(6),dp(8));contentDescription="修改途经点${index+1}";setOnClickListener{edit(index)}
                },LinearLayout.LayoutParams(0,dp(52),1f))
                listOf("↑","↓","×").forEachIndexed{k,title->
                    val item=button(title){
                        when(k){
                            0->if(index>0){val value=draft.removeAt(index);draft.add(index-1,value)}
                            1->if(index<draft.lastIndex){val value=draft.removeAt(index);draft.add(index+1,value)}
                            2->draft.removeAt(index)
                        };render()
                    }.apply{
                        contentDescription="${listOf("上移","下移","删除")[k]}途经点${index+1}"
                        isEnabled=when(k){0->index>0;1->index<draft.lastIndex;else->true};alpha=if(isEnabled)1f else .3f
                    }
                    row.addView(item,LinearLayout.LayoutParams(dp(34),dp(38)).apply{leftMargin=dp(4)})
                }
                rows.addView(row)
            }
            add.isEnabled=draft.size<MAX;add.alpha=if(add.isEnabled)1f else .4f
            note.text=if(draft.size>MAX)"当前导入 ${draft.size} 个地点，编辑后最多保留 $MAX 个" else "按列表顺序经过 · ${draft.size}/$MAX"
        }
        render()
        val dialog=AlertDialog.Builder(a).setTitle("途经点").setView(box)
            .setPositiveButton("应用途经点",null).setNegativeButton("取消",null).create()
        dialog.setOnShowListener{dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener{
            if(draft.size>MAX){Toast.makeText(a,"最多保留 $MAX 个途经点",Toast.LENGTH_SHORT).show();return@setOnClickListener}
            if(draft!=points)apply(draft.toList())
            dialog.dismiss()
        }}
        dialog.show()
    }
    private fun pick(a:AppCompatActivity,title:String,near:MotorPoint?,choose:(MotorPoint)->Unit){
        val s=NavStyle;fun dp(n:Int)=s.dpi(a,n)
        val box=LinearLayout(a).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(18),dp(8),dp(18),0)}
        val line=LinearLayout(a)
        val input=EditText(a).apply{hint="输入途经地点名称";isSingleLine=true;imeOptions=EditorInfo.IME_ACTION_SEARCH}
        line.addView(input,LinearLayout.LayoutParams(0,dp(48),1f))
        val search=TextView(a).apply{text="搜索";textSize=14f;setTextColor(s.CYAN);gravity=Gravity.CENTER}
        line.addView(search,LinearLayout.LayoutParams(dp(58),dp(48)));box.addView(line)
        val fuel=TextView(a).apply{
            text="搜索加油站";textSize=13f;setTextColor(s.CYAN);gravity=Gravity.CENTER
            background=s.round(a,s.CARD2,10f);contentDescription="搜索加油站"
        }
        box.addView(fuel,LinearLayout.LayoutParams(-1,dp(36)).apply{topMargin=dp(5)})
        val note=TextView(a).apply{textSize=12f;setTextColor(s.SUB);setPadding(0,dp(5),0,dp(5))};box.addView(note)
        val list=LinearLayout(a).apply{orientation=LinearLayout.VERTICAL}
        box.addView(ScrollView(a).apply{addView(list)},LinearLayout.LayoutParams(-1,dp(if(a.resources.displayMetrics.heightPixels/a.resources.displayMetrics.density<700)160 else 235)))
        val dialog=AlertDialog.Builder(a).setTitle(title).setView(ScrollView(a).apply{addView(box)}).setNegativeButton("取消",null).create()
        var revision=0
        fun add(point:MotorPoint,address:String=""){
            list.addView(TextView(a).apply{
                text=point.name+if(address.isNotBlank())"\n$address" else "";textSize=14f;setTextColor(s.TEXT)
                setPadding(0,dp(10),0,dp(10));maxLines=3
                setOnClickListener{choose(point);dialog.dismiss()}
            },LinearLayout.LayoutParams(-1,-2))
        }
        fun saved(){
            list.removeAllViews()
            val favorites=FavoritePlaces.load(a).map{MotorPoint(it.lat,it.lng,it.name)}
            val recent=MotorPlanHistory.load(a).flatMap{it.request.vias+listOf(it.request.start,it.request.end)}
            (favorites+recent).distinctBy{"${it.lat}:${it.lng}"}.take(12).forEach{add(it)}
            note.text=if(list.childCount>0)"可选常用／最近地点，也可以搜索" else "输入名称搜索，再选择具体地点"
        }
        fun query(){
            val keyword=input.text.toString().trim();if(keyword.isEmpty())return
            val token=++revision;list.removeAllViews();note.text="正在搜索…"
            if(!Keys.hasAmap(a)){note.text="请先在地图 Key 页面配置高德 Key";return}
            runCatching{
                Inputtips(a,InputtipsQuery(keyword,"").apply{
                    setCityLimit(false)
                    near?.takeIf{MotorTrack.valid(it.lat,it.lng)}?.let{setLocation(LatLonPoint(it.lat,it.lng))}
                }).apply{
                    setInputtipsListener(object:Inputtips.InputtipsListener{
                        override fun onGetInputtips(result:MutableList<Tip>?,code:Int){
                            if(a.isDestroyed||!dialog.isShowing||token!=revision||keyword!=input.text.toString().trim())return
                            list.removeAllViews()
                            if(code!=1000){note.text="搜索失败（$code），请检查网络和高德 Key";return}
                            val tips=result.orEmpty().filter{it.point!=null&&MotorTrack.valid(it.point.latitude,it.point.longitude)&&(it.point.latitude!=0.0||it.point.longitude!=0.0)}.take(20)
                            note.text=if(tips.isEmpty())"没有找到具体地点，请补充城市或完整名称" else "选择要经过的地点"
                            tips.forEach{add(MotorPoint(it.point.latitude,it.point.longitude,it.name.orEmpty(),it.poiID.orEmpty()),"${it.district.orEmpty()} ${it.address.orEmpty()}")}
                        }
                    });requestInputtipsAsyn()
                }
            }.onFailure{note.text="地点搜索失败，请稍后重试"}
        }
        search.setOnClickListener{
            (a.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).hideSoftInputFromWindow(input.windowToken,0)
            query()
        }
        fuel.setOnClickListener{
            input.setText("加油站")
            (a.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).hideSoftInputFromWindow(input.windowToken,0)
            query()
        }
        input.setOnEditorActionListener{_,action,_->if(action==EditorInfo.IME_ACTION_SEARCH){query();true}else false}
        input.addTextChangedListener(object:TextWatcher{
            override fun beforeTextChanged(t:CharSequence?,start:Int,count:Int,after:Int){}
            override fun onTextChanged(t:CharSequence?,start:Int,before:Int,count:Int){revision++;list.removeAllViews();if(t.isNullOrBlank())saved()}
            override fun afterTextChanged(t:Editable?){}
        })
        dialog.setOnDismissListener{revision++};saved();dialog.show()
        dialog.window?.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE or WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN)
    }
}
