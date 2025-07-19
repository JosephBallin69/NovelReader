# download_manager.py - Clean NovelFire downloader
import json
import os
import sys
import time
import argparse
from typing import List, Dict, Optional
import cloudscraper
from bs4 import BeautifulSoup
import requests
from urllib.parse import urljoin, urlparse
import re

class NovelDownloader:
    def __init__(self, source_config_path: str, quiet: bool = True):
        # Create cloudscraper with browser-like settings
        self.scraper = cloudscraper.create_scraper(
            browser={
                'browser': 'chrome',
                'platform': 'windows',
                'desktop': True
            }
        )
        
        # Add headers to look more like a real browser
        self.scraper.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
            'Upgrade-Insecure-Requests': '1',
        })
        
        self.session = self.scraper
        self.quiet = True  # Always quiet
        self.load_source_config(source_config_path)
    
    def _log(self, message: str):
        """Log message only if not in quiet mode"""
        pass  # Always quiet, no logging
        if not self.quiet:
            print(message, file=sys.stderr)
    
    def load_source_config(self, config_path: str):
        """Load source configuration from JSON file"""
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                self.config = json.load(f)
        except Exception as e:
            self._log(f"Error loading config: {e}")
            self.config = {"sources": []}
    
    def novel_name_to_url(self, novel_name: str, source_name: str) -> str:
        """Convert a novel name to the full URL for the given source"""
        source = next((s for s in self.config.get('sources', []) if s['name'] == source_name), None)
        if not source:
            raise ValueError(f"Unknown source: {source_name}")
        
        # Convert novel name to URL slug
        slug = novel_name.lower().replace(' ', '-')
        slug = re.sub(r'[^a-z0-9\-]', '', slug)
        slug = re.sub(r'-+', '-', slug)
        slug = slug.strip('-')
        
        return f"{source['base_url']}/book/{slug}"
    
    def _clean_text_for_json(self, text: str) -> str:
        """Clean text to ensure valid JSON encoding"""
        if not text:
            return ""
        
        # Replace problematic characters
        text = text.replace('–', '-')  # em dash
        text = text.replace('"', '"')  # smart quotes
        text = text.replace('"', '"')  # smart quotes
        text = text.replace(''', "'")  # smart apostrophe
        text = text.replace(''', "'")  # smart apostrophe
        text = text.replace('…', '...')  # ellipsis
        
        # Remove any non-ASCII characters that might cause issues
        text = text.encode('ascii', 'ignore').decode('ascii')
        
        # Clean up extra whitespace
        text = ' '.join(text.split())
        
        return text.strip()
    
    def search_novels(self, query: str) -> List[Dict]:
        """Search for novels by converting name to URL and checking if it exists"""
        results = []
        
        try:
            source = next((s for s in self.config.get('sources', []) if s['name'] == 'NovelFire'), None)
            if not source:
                return []
            
            # Convert query directly to URL and check if it exists
            try:
                novel_url = self.novel_name_to_url(query, 'NovelFire')
                response = self.session.get(novel_url, timeout=30)
                
                if response.status_code == 200:
                    info = self.get_novel_info(novel_url, 'NovelFire')
                    if info['title']:
                        result = {
                            "title": self._clean_text_for_json(info['title']),
                            "author": self._clean_text_for_json(info['author']),
                            "url": novel_url,
                            "description": self._clean_text_for_json(info['description']),
                            "source_name": "NovelFire",
                            "total_chapters": info['total_chapters'],
                            "cover_url": info['cover_url']
                        }
                        results.append(result)
            except Exception:
                pass
                
        except Exception:
            pass
        
        return results
    
    def _parse_search_item(self, item, source) -> Dict:
        """Parse individual search result item"""
        result = {
            "title": "",
            "author": "",
            "url": "",
            "description": "",
            "source_name": "NovelFire",
            "total_chapters": 0,
            "cover_url": ""
        }
        
        # Try to find title and link
        title_selectors = [
            'a[href*="/book/"]',  # Links to book pages
            '.title a', '.novel-title a', '.book-title a',
            'h3 a', 'h4 a', 'h5 a',
            'a[title]'  # Any link with title attribute
        ]
        
        title_elem = None
        for selector in title_selectors:
            title_elem = item.select_one(selector)
            if title_elem:
                break
        
        if title_elem:
            result['title'] = title_elem.get_text(strip=True)
            href = title_elem.get('href', '')
            if href:
                result['url'] = urljoin(source['base_url'], href)
        
        # Try to find author
        author_selectors = [
            '.author', '.novel-author', '.book-author',
            '[class*="author"]', '.by', '.writer'
        ]
        
        for selector in author_selectors:
            author_elem = item.select_one(selector)
            if author_elem:
                result['author'] = author_elem.get_text(strip=True)
                break
        
        # Try to find description
        desc_selectors = [
            '.description', '.summary', '.synopsis',
            '.novel-desc', '.book-desc', '[class*="desc"]'
        ]
        
        for selector in desc_selectors:
            desc_elem = item.select_one(selector)
            if desc_elem:
                result['description'] = desc_elem.get_text(strip=True)
                break
        
        # Try to find cover image
        cover_selectors = [
            'img[src*=".jpg"]', 'img[src*=".png"]', 'img[src*=".webp"]',
            '.cover img', '.image img', '.thumbnail img'
        ]
        
        for selector in cover_selectors:
            cover_elem = item.select_one(selector)
            if cover_elem:
                cover_src = cover_elem.get('src') or cover_elem.get('data-src')
                if cover_src:
                    result['cover_url'] = urljoin(source['base_url'], cover_src)
                    break
        
        return result
    
    def get_novel_info(self, novel_url: str, source_name: str) -> Dict:
        """Get detailed information about a novel with chapter URLs"""
        source = next((s for s in self.config.get('sources', []) if s['name'] == source_name), None)
        if not source:
            available_sources = [s['name'] for s in self.config.get('sources', [])]
            raise ValueError(f"Unknown source: {source_name}. Available sources: {available_sources}")
        
        self._log(f"Fetching novel info from: {novel_url}")
        
        try:
            response = self.session.get(novel_url, timeout=30)
            response.raise_for_status()
        except Exception as e:
            self._log(f"Error fetching novel page: {e}")
            raise
        
        soup = BeautifulSoup(response.content, 'html.parser')
        
        # Extract novel information
        info = {
            'title': '',
            'author': '',
            'description': '',
            'cover_url': '',
            'total_chapters': 0,
            'chapter_urls': []
        }
        
        # Get title
        title_elem = soup.select_one(source.get('selectors', {}).get('novel_title', 'h1'))
        if title_elem:
            info['title'] = title_elem.get_text(strip=True)
            self._log(f"Found title: {info['title']}")
        else:
            # Try alternative title selectors
            alt_title_selectors = ['h1', '.title', '[class*="title"]', 'title']
            for selector in alt_title_selectors:
                elem = soup.select_one(selector)
                if elem:
                    title_text = elem.get_text(strip=True)
                    if len(title_text) > 0 and len(title_text) < 200:  # Reasonable title length
                        info['title'] = title_text
                        self._log(f"Found title via alternative selector: {info['title']}")
                        break
        
        # Get author
        author_elem = soup.select_one(source.get('selectors', {}).get('novel_author', '.author'))
        if author_elem:
            info['author'] = author_elem.get_text(strip=True)
            self._log(f"Found author: {info['author']}")
        else:
            # Try alternative author selectors
            alt_author_selectors = ['.author', '[class*="author"]', '.by', '.writer']
            for selector in alt_author_selectors:
                elem = soup.select_one(selector)
                if elem:
                    author_text = elem.get_text(strip=True)
                    if author_text and not author_text.lower().startswith('author'):
                        info['author'] = author_text
                        self._log(f"Found author via alternative selector: {info['author']}")
                        break
        
        # Get description
        desc_elems = soup.select(source.get('selectors', {}).get('novel_description', '.description'))
        if desc_elems:
            descriptions = []
            for elem in desc_elems:
                text = elem.get_text(strip=True)
                if text:
                    descriptions.append(text)
            info['description'] = ' '.join(descriptions)
            self._log(f"Found description: {info['description'][:100]}...")
        else:
            # Try alternative description selectors
            alt_desc_selectors = ['.summary', '.synopsis', '.description', '[class*="desc"]', '.content p']
            for selector in alt_desc_selectors:
                elems = soup.select(selector)
                if elems:
                    descriptions = []
                    for elem in elems:
                        text = elem.get_text(strip=True)
                        if text and len(text) > 20:  # Substantial text
                            descriptions.append(text)
                    if descriptions:
                        info['description'] = ' '.join(descriptions)
                        self._log(f"Found description via alternative selector: {info['description'][:100]}...")
                        break
        
        # Get cover
        cover_elem = soup.select_one(source.get('selectors', {}).get('novel_cover', 'img'))
        if cover_elem:
            cover_src = cover_elem.get('src') or cover_elem.get('data-src')
            if cover_src:
                info['cover_url'] = urljoin(source['base_url'], cover_src)
                self._log(f"Found cover: {info['cover_url']}")
        
        # Get chapter count and URLs
        info['total_chapters'] = self._extract_chapter_count(soup)
        info['chapter_urls'] = self._extract_chapter_urls(novel_url, source, info['total_chapters'])
        self._log(f"Using total chapters: {info['total_chapters']}")
        
        return info
    
    def _extract_chapter_count(self, soup) -> int:
        """Extract total chapter count from novel page"""
        # Method 1: Look for "Chapters" text with number
        chapters_span = soup.find('small', string=re.compile(r'Chapters', re.I))
        if chapters_span and chapters_span.parent:
            strong_elem = chapters_span.parent.find('strong')
            if strong_elem:
                numbers = re.findall(r'\d+', strong_elem.get_text(strip=True))
                if numbers:
                    return int(numbers[0])
        
        # Method 2: Look for book icon with chapter count
        book_icon = soup.find('i', class_='icon-book-open')
        if book_icon and book_icon.parent:
            text = book_icon.parent.get_text(strip=True)
            numbers = re.findall(r'\d+', text)
            if numbers:
                return int(numbers[0])
        
        # Method 3: Pattern matching in page text
        page_text = soup.get_text()
        chapter_pattern = r'(\d{3,})\s*Chapters?'
        matches = re.findall(chapter_pattern, page_text, re.IGNORECASE)
        if matches:
            return int(matches[0])
        
        # Default fallback
        return 3000
    
    def _extract_chapter_urls(self, novel_url: str, source: Dict, total_chapters: int) -> List[Dict]:
        """Generate chapter URLs based on novel URL pattern"""
        chapter_urls = []
        novel_slug = novel_url.split('/book/')[-1].rstrip('/')
        
        for i in range(1, min(total_chapters + 1, 3001)):  # Limit to prevent excessive URLs
            chapter_url = f"{source['base_url']}/book/{novel_slug}/chapter-{i}"
            chapter_urls.append({
                "number": i,
                "title": f"Chapter {i}",
                "url": chapter_url
            })
        
        return chapter_urls
    
    def download_chapter(self, chapter_url: str, source_name: str, chapter_number: int) -> Dict:
        """Download a single chapter"""
        source = next((s for s in self.config.get('sources', []) if s['name'] == source_name), None)
        if not source:
            raise ValueError(f"Unknown source: {source_name}")
        
        self._log(f"Downloading chapter {chapter_number}: {chapter_url}")
        
        try:
            response = self.session.get(chapter_url, timeout=30)
            response.raise_for_status()
        except Exception as e:
            self._log(f"Error fetching chapter {chapter_number}: {e}")
            raise
        
        soup = BeautifulSoup(response.content, 'html.parser')
        
        # Extract chapter title
        title_elem = soup.select_one(source.get('selectors', {}).get('chapter_title', 'h1'))
        chapter_title = f"Chapter {chapter_number}"
        if title_elem:
            raw_title = title_elem.get_text(strip=True)
            cleaned_title = re.sub(r'^Chapter\s+\d+\s*[-:]?\s*', '', raw_title, flags=re.IGNORECASE)
            if cleaned_title:
                chapter_title = f"Chapter {chapter_number} {cleaned_title}"
            else:
                chapter_title = raw_title if raw_title else f"Chapter {chapter_number}"
        
        # Extract chapter content
        content_elem = soup.select_one(source.get('selectors', {}).get('chapter_content', '.content'))
        if not content_elem:
            raise ValueError(f"Could not find chapter content for chapter {chapter_number}")
        
        # Clean content
        content = self._clean_content(content_elem, source)
        
        # Remove title from content if it appears at the beginning
        if title_elem:
            title_text = title_elem.get_text(strip=True)
            if content.startswith(title_text):
                content = content[len(title_text):].lstrip('\n')
        
        if not content or len(content.strip()) < 100:
            raise ValueError(f"Chapter {chapter_number} content too short or empty")
        
        return {
            'title': chapter_title,
            'content': content
        }
    
    def _clean_content(self, content_elem, source: Dict) -> str:
        """Clean and format chapter content"""
        content_copy = BeautifulSoup(str(content_elem), 'html.parser')
        
        # Remove unwanted elements
        for selector in source.get('selectors', {}).get('remove_selectors', []):
            try:
                for elem in content_copy.select(selector):
                    elem.decompose()
            except Exception:
                pass
        
        # Extract paragraphs
        content_lines = []
        for elem in content_copy.find_all('p', recursive=True):
            text = elem.get_text(strip=True)
            if text and len(text) > 10:
                cleaned_text = self._clean_text(text)
                if cleaned_text:
                    content_lines.append(cleaned_text)
        
        # If no paragraphs, try splitting all text
        if not content_lines:
            all_text = content_copy.get_text(separator='\n', strip=True)
            if all_text:
                for para in all_text.split('\n\n'):
                    cleaned = self._clean_text(para.strip())
                    if cleaned and len(cleaned) > 10:
                        content_lines.append(cleaned)
        
        # Join with proper spacing and final cleanup
        content = '\n\n'.join(content_lines)
        return self._final_content_cleanup(content)
    
    def _clean_text(self, text: str) -> str:
        """Clean individual text segments"""
        if not text:
            return ""
        
        # Remove extra whitespace
        text = ' '.join(text.split())
        
        # Remove unwanted patterns
        unwanted_patterns = [
            r'^Chapter \d+.*?$',
            r'^[\d\s]*$',
            r'^\s*\*{3,}\s*$',
            r'^\s*-{3,}\s*$',
            r'^\s*_{3,}\s*$',
        ]
        
        for pattern in unwanted_patterns:
            if re.match(pattern, text, re.IGNORECASE):
                return ""
        
        # Skip very short lines
        if len(text) < 10:
            return ""
        
        return text
    
    def _final_content_cleanup(self, content: str) -> str:
        """Final content cleanup"""
        if not content:
            return ""
        
        # Remove duplicate consecutive paragraphs
        lines = content.split('\n\n')
        unique_lines = []
        prev_line = ""
        
        for line in lines:
            line = line.strip()
            if line and line != prev_line:
                unique_lines.append(line)
                prev_line = line
        
        content = '\n\n'.join(unique_lines)
        content = re.sub(r'\n{3,}', '\n\n', content)
        
        # Clean HTML entities
        html_entities = {
            '&nbsp;': ' ', '&amp;': '&', '&lt;': '<', 
            '&gt;': '>', '&quot;': '"', '&#39;': "'"
        }
        for entity, replacement in html_entities.items():
            content = content.replace(entity, replacement)
        
        return content.strip()
    
    def download_novel(self, novel_url: str, source_name: str, output_dir: str, 
                      start_chapter: int = 1, end_chapter: int = -1, 
                      progress_callback=None) -> bool:
        """Download entire novel or chapter range"""
        try:
            source = next((s for s in self.config.get('sources', []) if s['name'] == source_name), None)
            if not source:
                available_sources = [s['name'] for s in self.config.get('sources', [])]
                raise ValueError(f"Unknown source: {source_name}. Available sources: {available_sources}")
            
            # Get novel info
            self._log("Getting novel information...")
            info = self.get_novel_info(novel_url, source_name)
            
            if not info['title']:
                self._log("Error: Could not extract novel title")
                return False
            
            # Create directories
            novel_dir = os.path.join(output_dir, self._sanitize_filename(info['title']))
            chapters_dir = os.path.join(novel_dir, 'chapters')
            os.makedirs(chapters_dir, exist_ok=True)
            self._log(f"Created directory: {novel_dir}")
            
            # Download cover
            if info['cover_url']:
                self._log("Downloading cover...")
                self._download_cover(info['cover_url'], novel_dir)
            
            # Update Novels.json
            self._update_novels_json(output_dir, info, novel_dir)
            
            # Determine chapter range
            if end_chapter == -1:
                end_chapter = info['total_chapters']
            end_chapter = min(end_chapter, info['total_chapters'])
            
            self._log(f"Downloading chapters {start_chapter} to {end_chapter}")
            
            # Download chapters
            downloaded_count = 0
            novel_slug = novel_url.split('/book/')[-1].rstrip('/')
            
            for i in range(start_chapter, end_chapter + 1):
                chapter_url = f"{source['base_url']}/book/{novel_slug}/chapter-{i}"
                chapter_file = os.path.join(chapters_dir, f"chapter{i}.json")
                
                # Skip if exists
                if os.path.exists(chapter_file):
                    self._log(f"Chapter {i} already exists, skipping...")
                    continue
                
                try:
                    chapter_data = self.download_chapter(chapter_url, source_name, i)
                    
                    chapter = {
                        'chapterNumber': i,
                        'title': chapter_data['title'],
                        'content': chapter_data['content']
                    }
                    
                    # Save chapter
                    with open(chapter_file, 'w', encoding='utf-8') as f:
                        json.dump(chapter, f, indent=2, ensure_ascii=False)
                    
                    downloaded_count += 1
                    self._log(f"✓ Downloaded chapter {i}: {chapter_data['title']}")
                    
                    if progress_callback:
                        progress_callback(i, end_chapter, chapter_data['title'])
                    
                    # Rate limiting
                    if downloaded_count % 10 == 0:
                        self._log(f"Downloaded {downloaded_count} chapters, waiting 15 seconds...")
                        time.sleep(15)
                    else:
                        time.sleep(1)
                    
                except Exception as e:
                    self._log(f"✗ Error downloading chapter {i}: {e}")
                    continue
            
            self._log(f"Download completed! Downloaded {downloaded_count} chapters to {novel_dir}")
            return True
            
        except Exception as e:
            self._log(f"Error downloading novel: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def _update_novels_json(self, output_dir: str, info: Dict, novel_dir: str):
        """Update the Novels.json file"""
        novels_json_path = os.path.join(output_dir, 'Novels.json')
        novels_data = {"novels": []}
        
        # Load existing
        if os.path.exists(novels_json_path):
            try:
                with open(novels_json_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                if isinstance(data, dict) and 'novels' in data and isinstance(data['novels'], list):
                    novels_data = data
            except:
                pass
        
        # Create novel info
        novel_info = {
            'name': info['title'],
            'authorname': info['author'],
            'coverpath': os.path.join(novel_dir, 'cover.jpg'),
            'synopsis': info['description'],
            'totalchapters': info['total_chapters'],
            'progress': {
                'readchapters': 0,
                'progresspercentage': 0.0
            }
        }
        
        # Update or add
        existing_idx = None
        for i, novel in enumerate(novels_data["novels"]):
            if isinstance(novel, dict) and novel.get('name') == info['title']:
                existing_idx = i
                break
        
        if existing_idx is not None:
            novels_data["novels"][existing_idx] = novel_info
        else:
            novels_data["novels"].append(novel_info)
        
        # Save
        with open(novels_json_path, 'w', encoding='utf-8') as f:
            json.dump(novels_data, f, indent=4, ensure_ascii=False)
    
    def _sanitize_filename(self, filename: str) -> str:
        """Sanitize filename for filesystem"""
        return re.sub(r'[<>:"/\\|?*]', '_', filename)
    
    def _download_cover(self, cover_url: str, output_dir: str):
        """Download cover image"""
        try:
            self._log(f"Downloading cover from: {cover_url}")
            response = self.session.get(cover_url, timeout=30)
            response.raise_for_status()
            
            os.makedirs(output_dir, exist_ok=True)
            cover_path = os.path.join(output_dir, 'cover.jpg')
            
            with open(cover_path, 'wb') as f:
                f.write(response.content)
            
            self._log(f"✓ Downloaded cover to {cover_path}")
            
        except Exception as e:
            self._log(f"✗ Error downloading cover: {e}")

def main():
    parser = argparse.ArgumentParser(description='Novel Download Manager')
    parser.add_argument('action', choices=['search', 'download', 'info'])
    parser.add_argument('--query', help='Search query')
    parser.add_argument('--url', help='Novel URL (full URL)')
    parser.add_argument('--name', help='Novel name (will be converted to URL)')
    parser.add_argument('--source', help='Source name')
    parser.add_argument('--output', help='Output directory', default='Novels')
    parser.add_argument('--start', type=int, default=1, help='Start chapter')
    parser.add_argument('--end', type=int, default=-1, help='End chapter (-1 for all)')
    parser.add_argument('--config', help='Source config file', default='sources.json')
    
    args = parser.parse_args()
    downloader = NovelDownloader(args.config, quiet=True)
    
    if args.action == 'search':
        if not args.query:
            result = []
            print(json.dumps(result, ensure_ascii=False))
            sys.exit(1)
        
        results = downloader.search_novels(args.query)
        print(json.dumps(results, ensure_ascii=True))
    
    elif args.action == 'info':
        novel_url = args.url
        if not novel_url and args.name and args.source:
            try:
                novel_url = downloader.novel_name_to_url(args.name, args.source)
            except Exception as e:
                error_result = {"error": f"Error converting novel name to URL: {e}"}
                print(json.dumps(error_result, ensure_ascii=False))
                sys.exit(1)
        
        if not novel_url or not args.source:
            error_result = {"error": "Either --url or --name (with --source) required for info"}
            print(json.dumps(error_result, ensure_ascii=False))
            sys.exit(1)
        
        try:
            info = downloader.get_novel_info(novel_url, args.source)
            print(json.dumps(info, ensure_ascii=True))
        except Exception as e:
            error_result = {"error": f"Error getting novel info: {e}"}
            print(json.dumps(error_result, ensure_ascii=False))
            sys.exit(1)
    
    elif args.action == 'download':
        novel_url = args.url
        if not novel_url and args.name and args.source:
            try:
                novel_url = downloader.novel_name_to_url(args.name, args.source)
            except Exception as e:
                print(f"Error converting novel name to URL: {e}", file=sys.stderr)
                sys.exit(1)
        
        if not novel_url or not args.source:
            print("Either --url or --name (with --source) required for download", file=sys.stderr)
            sys.exit(1)
        
        def progress_callback(current, total, chapter_title):
            percentage = (current / total) * 100
            print(f"Progress: {current}/{total} ({percentage:.1f}%) - {chapter_title}", file=sys.stderr)
        
        success = downloader.download_novel(
            novel_url, args.source, args.output,
            args.start, args.end, progress_callback
        )
        
        if not success:
            sys.exit(1)

if __name__ == '__main__':
    main()