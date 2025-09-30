#!/usr/bin/env python3
"""
Merge No-Intro and Sanni databases into compatible format
Creates gb.txt and gba.txt files for use with cart readers

Database formats:
- Sanni GB format (gb.txt):
  Line 1: Game name
  Line 2: CRC32 (8 hex chars uppercase)
  Line 3: Additional info (size, etc)
  
- Sanni GBA format (gba.txt):
  XXXX,YY,Z
  Where: XXXX=Game ID, YY=ROM size in MB, Z=Save type
"""

import xml.etree.ElementTree as ET
import hashlib
import re
import sys
import os
from pathlib import Path

def parse_no_intro_dat(dat_file):
    """Parse No-Intro DAT file (XML format)"""
    games = []
    
    try:
        tree = ET.parse(dat_file)
        root = tree.getroot()
        
        for game in root.findall('.//game'):
            name = game.get('name', '')
            
            # Get ROM info
            for rom in game.findall('rom'):
                rom_name = rom.get('name', '')
                size = int(rom.get('size', '0'))
                crc32 = rom.get('crc', '').upper()
                md5 = rom.get('md5', '').upper()
                sha1 = rom.get('sha1', '').upper()
                
                games.append({
                    'name': name,
                    'rom_name': rom_name,
                    'size': size,
                    'crc32': crc32,
                    'md5': md5,
                    'sha1': sha1
                })
                
    except Exception as e:
        print(f"Error parsing DAT file: {e}")
    
    return games

def parse_sanni_gb_txt(txt_file):
    """Parse existing Sanni gb.txt format"""
    games = []
    
    try:
        with open(txt_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
        i = 0
        while i < len(lines) - 2:
            name = lines[i].strip()
            crc32 = lines[i+1].strip().upper()
            info = lines[i+2].strip()
            
            if len(crc32) == 8:  # Valid CRC32
                games.append({
                    'name': name,
                    'crc32': crc32,
                    'info': info
                })
            
            i += 3
            
    except Exception as e:
        print(f"Error parsing Sanni file: {e}")
    
    return games

def parse_sanni_gba_txt(txt_file):
    """Parse existing Sanni gba.txt format"""
    games = []
    
    try:
        with open(txt_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if len(line) >= 9 and line[4] == ',' and line[7] == ',':
                    game_id = line[0:4]
                    rom_size = line[5:7]
                    save_type = line[8]
                    
                    games.append({
                        'game_id': game_id,
                        'rom_size': int(rom_size),
                        'save_type': int(save_type)
                    })
                    
    except Exception as e:
        print(f"Error parsing Sanni GBA file: {e}")
    
    return games

def merge_gb_databases(no_intro_games, sanni_games):
    """Merge GB/GBC databases, removing duplicates"""
    merged = {}
    
    # Add Sanni games first (they have known good format)
    for game in sanni_games:
        merged[game['crc32']] = {
            'name': game['name'],
            'crc32': game['crc32'],
            'info': game.get('info', 'NoIntro')
        }
    
    # Add No-Intro games (may have better names)
    for game in no_intro_games:
        crc32 = game['crc32']
        if crc32 and len(crc32) == 8:
            if crc32 not in merged:
                # Clean up the name
                name = game['name']
                # Remove region codes like (USA), (Japan), etc
                name = re.sub(r'\s*\([^)]*\)\s*', ' ', name).strip()
                # Limit length for display
                if len(name) > 60:
                    name = name[:57] + "..."
                    
                size_mb = game['size'] / (1024 * 1024)
                info = f"{size_mb:.1f}MB NoIntro"
                
                merged[crc32] = {
                    'name': name,
                    'crc32': crc32,
                    'info': info
                }
    
    return list(merged.values())

def write_gb_database(games, output_file):
    """Write database in Sanni gb.txt format"""
    with open(output_file, 'w', encoding='utf-8') as f:
        for game in sorted(games, key=lambda x: x['name']):
            f.write(f"{game['name']}\n")
            f.write(f"{game['crc32']}\n")
            f.write(f"{game['info']}\n")
    
    print(f"Wrote {len(games)} GB/GBC games to {output_file}")

def extract_gba_info_from_no_intro(no_intro_games):
    """Extract GBA game info from No-Intro data"""
    # This is a simplified version - in reality, you'd need to:
    # 1. Read the actual GBA ROM headers to get the 4-char game ID
    # 2. Map save types based on game-specific databases
    # For now, this creates a basic entry
    gba_games = []
    
    for game in no_intro_games:
        # Try to extract a 4-char code from the ROM name
        rom_name = game.get('rom_name', '')
        match = re.search(r'([A-Z0-9]{4})', rom_name)
        
        if match:
            game_id = match.group(1)
            size_mb = game['size'] / (1024 * 1024)
            
            # Guess save type based on size (this is not accurate!)
            # Real implementation would need game-specific database
            if size_mb <= 4:
                save_type = 3  # SRAM
            elif size_mb <= 8:
                save_type = 4  # Flash 64K
            else:
                save_type = 5  # Flash 128K
                
            gba_games.append({
                'game_id': game_id,
                'rom_size': int(size_mb),
                'save_type': save_type,
                'name': game['name']
            })
    
    return gba_games

def merge_gba_databases(no_intro_games, sanni_games):
    """Merge GBA databases"""
    merged = {}
    
    # Add Sanni games first
    for game in sanni_games:
        key = game['game_id']
        merged[key] = game
    
    # Add No-Intro games if we can extract info
    # Note: This is simplified - real implementation needs ROM header reading
    no_intro_gba = extract_gba_info_from_no_intro(no_intro_games)
    for game in no_intro_gba:
        if game['game_id'] not in merged:
            merged[game['game_id']] = game
    
    return list(merged.values())

def write_gba_database(games, output_file):
    """Write database in Sanni gba.txt format"""
    with open(output_file, 'w', encoding='utf-8') as f:
        for game in sorted(games, key=lambda x: x['game_id']):
            f.write(f"{game['game_id']},{game['rom_size']:02d},{game['save_type']}\n")
    
    print(f"Wrote {len(games)} GBA games to {output_file}")

def main():
    """Main function to merge databases"""
    print("Database Merger for Burnmaster/Sanni Cart Reader")
    print("=" * 50)
    
    # Check for input files
    no_intro_gb = "Nintendo - Game Boy.dat"
    no_intro_gbc = "Nintendo - Game Boy Color.dat"
    no_intro_gba = "Nintendo - Game Boy Advance.dat"
    sanni_gb = "gb_sanni.txt"
    sanni_gba = "gba_sanni.txt"
    
    # Parse No-Intro databases
    all_gb_games = []
    
    if os.path.exists(no_intro_gb):
        print(f"Parsing {no_intro_gb}...")
        games = parse_no_intro_dat(no_intro_gb)
        all_gb_games.extend(games)
        print(f"  Found {len(games)} games")
    
    if os.path.exists(no_intro_gbc):
        print(f"Parsing {no_intro_gbc}...")
        games = parse_no_intro_dat(no_intro_gbc)
        all_gb_games.extend(games)
        print(f"  Found {len(games)} games")
    
    # Parse Sanni databases
    sanni_gb_games = []
    if os.path.exists(sanni_gb):
        print(f"Parsing {sanni_gb}...")
        sanni_gb_games = parse_sanni_gb_txt(sanni_gb)
        print(f"  Found {len(sanni_gb_games)} games")
    
    # Merge GB/GBC databases
    if all_gb_games or sanni_gb_games:
        print("\nMerging GB/GBC databases...")
        merged_gb = merge_gb_databases(all_gb_games, sanni_gb_games)
        write_gb_database(merged_gb, "gb.txt")
    
    # Handle GBA
    all_gba_games = []
    if os.path.exists(no_intro_gba):
        print(f"\nParsing {no_intro_gba}...")
        all_gba_games = parse_no_intro_dat(no_intro_gba)
        print(f"  Found {len(all_gba_games)} games")
    
    sanni_gba_games = []
    if os.path.exists(sanni_gba):
        print(f"Parsing {sanni_gba}...")
        sanni_gba_games = parse_sanni_gba_txt(sanni_gba)
        print(f"  Found {len(sanni_gba_games)} games")
    
    # Merge GBA databases
    if all_gba_games or sanni_gba_games:
        print("\nMerging GBA databases...")
        merged_gba = merge_gba_databases(all_gba_games, sanni_gba_games)
        write_gba_database(merged_gba, "gba.txt")
    
    print("\nDatabase merge complete!")
    print("\nNote: For accurate GBA database, you need:")
    print("  1. Actual ROM headers to extract 4-char game IDs")
    print("  2. A save type database (many games use specific save types)")
    print("  3. The Sanni gba.txt has this info for many games already")

if __name__ == "__main__":
    main()