package com.yuminaga.motonav;

import android.app.*;
import android.content.*;

/** One installed package; choose the actual Application before either engine initializes. */
public final class FusionComponentFactory extends AppComponentFactory {
    private static boolean moto(){String p=Application.getProcessName();return p!=null && p.endsWith(":motonav");}
    private AppComponentFactory delegate(ClassLoader loader) {
        String cls=moto()?"androidx.core.app.CoreComponentFactory":"carxlibx.core.app.CoreComponentFactory";
        try{return (AppComponentFactory)Class.forName(cls,true,loader).getConstructor().newInstance();}
        catch(ReflectiveOperationException e){throw new IllegalStateException("Fusion component factory "+cls,e);}
    }
    @Override public Application instantiateApplication(ClassLoader loader,String name)
        throws InstantiationException,IllegalAccessException,ClassNotFoundException {
        Application app=delegate(loader).instantiateApplication(loader,moto()?"com.yuminaga.motonav.MotoNavApp":name);
        if(!moto())FusionCarUi.install(app);
        return app;
    }
    @Override public Activity instantiateActivity(ClassLoader loader,String name,Intent intent)
        throws InstantiationException,IllegalAccessException,ClassNotFoundException {
        return delegate(loader).instantiateActivity(loader,name,intent);
    }
    @Override public Service instantiateService(ClassLoader loader,String name,Intent intent)
        throws InstantiationException,IllegalAccessException,ClassNotFoundException {
        return delegate(loader).instantiateService(loader,name,intent);
    }
    @Override public BroadcastReceiver instantiateReceiver(ClassLoader loader,String name,Intent intent)
        throws InstantiationException,IllegalAccessException,ClassNotFoundException {
        return delegate(loader).instantiateReceiver(loader,name,intent);
    }
    @Override public ContentProvider instantiateProvider(ClassLoader loader,String name)
        throws InstantiationException,IllegalAccessException,ClassNotFoundException {
        return delegate(loader).instantiateProvider(loader,name);
    }
}
