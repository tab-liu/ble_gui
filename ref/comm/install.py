import os
import shutil
import glob
import sys

def clear_operation(comp_output_dir, comp):
    """执行清理操作：删除.a文件和include目录中的.h文件"""
    # 删除.a文件
    lib_name = f"lib{comp}.a"
    lib_path = os.path.join(comp_output_dir, lib_name)
    if os.path.exists(lib_path):
        os.remove(lib_path)
        print(f"已删除旧库文件: {lib_path}")
    
    # 删除include目录中的所有.h文件，但保留目录结构和其他文件
    include_dir = os.path.join(comp_output_dir, "include")
    if os.path.exists(include_dir):
        # 递归查找所有.h文件并删除
        h_files = glob.glob(os.path.join(include_dir, "**", "*.h"), recursive=True)
        for h_file in h_files:
            os.remove(h_file)
            print(f"已删除旧头文件: {h_file}")

def install_operation(comp_output_dir, comp, components_dir):
    """执行安装操作：复制库文件和头文件"""
    # 复制静态库文件
    lib_name = f"lib{comp}.a"
    lib_src_path = os.path.join("build", "esp-idf", comp, lib_name)
    
    if os.path.exists(lib_src_path):
        dest_lib = os.path.join(comp_output_dir, lib_name)
        shutil.copy2(lib_src_path, dest_lib)
        print(f"已复制库文件: {lib_src_path} -> {dest_lib}")
    else:
        print(f"警告：库文件不存在 {lib_src_path}")
    
    # 递归复制所有头文件
    include_src = os.path.join(components_dir, comp)
    include_dest = os.path.join(comp_output_dir, "include")
    
    # 创建目标include目录
    os.makedirs(include_dest, exist_ok=True)
    
    # 递归查找所有.h文件
    h_files = glob.glob(os.path.join(include_src, "**", "*.h"), recursive=True)
    
    for src_file in h_files:
        # 计算相对路径
        rel_path = os.path.relpath(src_file, include_src)
        dest_file = os.path.join(include_dest, rel_path)
        
        # 确保目标目录存在
        os.makedirs(os.path.dirname(dest_file), exist_ok=True)
        
        shutil.copy2(src_file, dest_file)
        print(f"已复制头文件: {src_file} -> {dest_file}")

def main():
    # 解析命令行参数
    operation = "default"  # 默认操作：先clear后install
    if len(sys.argv) > 1:
        operation = sys.argv[1].lower()
    
    # 步骤1：遍历components目录下的所有子目录
    components_dir = "components"
    if not os.path.exists(components_dir):
        print(f"错误：目录 '{components_dir}' 不存在")
        return

    # 获取所有组件名称（仅目录）
    component_names = [
        name for name in os.listdir(components_dir)
        if os.path.isdir(os.path.join(components_dir, name))
    ]

    if not component_names:
        print("未找到任何组件目录")
        return

    # 步骤2：创建output目录及其子目录
    output_dir = "output"
    os.makedirs(output_dir, exist_ok=True)
    
    # 为每个组件创建输出目录
    for comp in component_names:
        comp_output_dir = os.path.join(output_dir, comp)
        os.makedirs(comp_output_dir, exist_ok=True)
        
        # 根据参数执行相应操作
        if operation == "clear":
            # 只执行清理操作
            clear_operation(comp_output_dir, comp)
        elif operation == "install":
            # 只执行安装操作
            install_operation(comp_output_dir, comp, components_dir)
        else:
            # 默认操作：先清理后安装
            clear_operation(comp_output_dir, comp)
            install_operation(comp_output_dir, comp, components_dir)

    print(f"操作完成！执行模式: {operation}")

if __name__ == "__main__":
    main()