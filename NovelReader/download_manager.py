#!/usr/bin/env python3
"""
download_manager.py - Universal content downloader for novels and manga
Works with the C++ Library management system
"""

import json
import os
import sys
import time
import argparse
import re
import logging
from datetime import datetime
from typing import List, Dict, Optional, Tuple
from enum import Enum
from dataclasses import dataclass
from urllib.parse import urljoin, urlparse, quote

# Try to import cloudscraper, fall back to requests if not available
try:
    import cloudscraper
    SESSION = cloudscraper.create_scraper(
        browser={
            'browser': 'chrome',
            'platform': 'windows',
            'desktop': True
        }
    )
except ImportError:
    import requests
    SESSION = requests.Session()

from bs4 import BeautifulSoup

# Set up logging - ONLY to stderr to avoid corrupting stdout JSON
logging.basicConfig(
    level=logging.INFO,
    format='%(message)s',
    stream=sys.stderr
)
logger = logging.getLogger(__name__)

class ContentType(Enum):
    ALL = "all"
    NOVEL = "novel"
    MANGA = "manga"
    MANHWA = "manhwa"
    MANHUA = "manhua"

@dataclass
class SearchResult:
    title: str
    author: str
    url: str
    source_name: str
    total_chapters: int
    description: str
    cover_url: str
    content_type: ContentType = ContentType.NOVEL

@dataclass
class ChapterProvider:
    name: str
    language: str
    url: str
    upload_date: str
    chapter_title: str
    flag_code: str = ""

class UniversalDownloader:
    def __init__(self, config_path: str = "sources.json"):
        self.session = SESSION
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
            'Upgrade-Insecure-Requests': '1',
        })
        
        self.sources = {}
        self.load_sources(config_path)
        self.download_states = {}
        self.should_stop = {}
    
    def load_sources(self, config_path: str):
        """Load source configurations"""
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
                for source in config.get('sources', []):
                    if source.get('enabled', True):
                        self.sources[source['name']] = source
        except Exception as e:
            logger.error(f"Error loading sources: {e}")
    
    def search_content(self, query: str, content_type: str = "all", 
                      language: str = "", include_adult: bool = False,
                      max_results_per_source: int = 2) -> List[Dict]:
        """Search for content across all enabled sources"""
        results = []
        
        for source_name, source in self.sources.items():
            try:
                # Check content type compatibility
                if content_type != "all":
                    source_types = source.get('content_types', ['novel'])
                    if content_type not in source_types:
                        continue
                
                logger.info(f"Searching {source_name} for: {query}")
                
                if source_name == "NovelFire":
                    source_results = self._search_novelfire(query, source)
                elif source_name == "Comick":
                    source_results = self._search_comick(query, source, content_type)
                else:
                    source_results = self._generic_search(query, source)
                
                # Limit results per source
                results.extend(source_results[:max_results_per_source])
                
            except Exception as e:
                logger.error(f"Error searching {source_name}: {e}")
                continue
        
        return results
    
    def _search_novelfire(self, query: str, source: Dict) -> List[Dict]:
        """Search NovelFire for novels"""
        results = []
        
        # First try direct URL construction
        slug = self._title_to_slug(query)
        novel_url = f"{source['base_url']}/book/{slug}"
        
        try:
            response = self.session.get(novel_url, timeout=30)
            if response.status_code == 200:
                info = self.get_content_info(novel_url, source['name'])
                if info and info.get('title'):
                    results.append({
                        "title": info['title'],
                        "author": info.get('author', 'Unknown'),
                        "url": novel_url,
                        "source_name": "NovelFire",
                        "total_chapters": info.get('total_chapters', 0),
                        "description": info.get('description', ''),
                        "cover_url": info.get('cover_url', '')
                    })
        except:
            pass
        
        # If direct URL didn't work, try search endpoint
        if not results:
            search_url = source['base_url'] + source.get('search_endpoint', '/search').replace('{query}', quote(query))
            
            try:
                response = self.session.get(search_url, timeout=30)
                response.raise_for_status()
                
                soup = BeautifulSoup(response.text, 'html.parser')
                selectors = source.get('selectors', {})
                
                items = soup.select(selectors.get('search_item', '.novel-item'))
                
                for item in items[:2]:  # Limit to 2 results
                    result = self._parse_search_item(item, source)
                    if result:
                        results.append(result)
                        
            except Exception as e:
                logger.error(f"Search error: {e}")
        
        return results
    
    def _search_comick(self, query: str, source: Dict, content_type: str) -> List[Dict]:
        """Search Comick for manga/manhwa/manhua"""
        results = []
        
        # Comick.io uses a different search pattern
        search_url = f"{source['base_url']}/search"
        params = {
            'q': query,
            'limit': '10'
        }
        
        try:
            response = self.session.get(search_url, params=params, timeout=30)
            response.raise_for_status()
            
            # Comick might return JSON or HTML
            if 'application/json' in response.headers.get('content-type', ''):
                data = response.json()
                # Parse JSON response
                for item in data.get('results', [])[:2]:
                    results.append({
                        "title": item.get('title', ''),
                        "author": item.get('author', 'Unknown'),
                        "url": f"{source['base_url']}/comic/{item.get('slug', '')}",
                        "source_name": "Comick",
                        "total_chapters": item.get('chapter_count', 0),
                        "description": item.get('description', ''),
                        "cover_url": item.get('cover_url', '')
                    })
            else:
                # Parse HTML response
                soup = BeautifulSoup(response.text, 'html.parser')
                items = soup.select('a[href*="/comic/"]')
                
                for item in items[:2]:
                    title = item.get_text(strip=True)
                    url = urljoin(source['base_url'], item.get('href', ''))
                    
                    results.append({
                        "title": title,
                        "author": "Unknown",
                        "url": url,
                        "source_name": "Comick",
                        "total_chapters": 0,
                        "description": "",
                        "cover_url": ""
                    })
                    
        except Exception as e:
            logger.error(f"Comick search error: {e}")
        
        return results
    
    def _generic_search(self, query: str, source: Dict) -> List[Dict]:
        """Generic search implementation"""
        results = []
        search_endpoint = source.get('search_endpoint', '/search?q={query}')
        search_url = source['base_url'] + search_endpoint.replace('{query}', quote(query))
        
        try:
            response = self.session.get(search_url, timeout=30)
            response.raise_for_status()
            
            soup = BeautifulSoup(response.text, 'html.parser')
            selectors = source.get('selectors', {})
            
            # Find search result items
            item_selector = selectors.get('search_item', '.search-item')
            items = soup.select(item_selector)
            
            for item in items[:2]:
                result = self._parse_search_item(item, source)
                if result:
                    results.append(result)
                    
        except Exception as e:
            logger.error(f"Generic search error: {e}")
        
        return results
    
    def _parse_search_item(self, item, source: Dict) -> Optional[Dict]:
        """Parse a search result item"""
        try:
            selectors = source.get('selectors', {})
            
            # Extract title and URL
            title_elem = item.select_one(selectors.get('title', 'a'))
            if not title_elem:
                return None
            
            title = title_elem.get_text(strip=True)
            link = title_elem.get('href', '')
            if not link:
                link_elem = item.select_one(selectors.get('link', 'a[href]'))
                if link_elem:
                    link = link_elem.get('href', '')
            
            if not link:
                return None
            
            url = urljoin(source['base_url'], link)
            
            # Extract other fields
            author = ""
            author_elem = item.select_one(selectors.get('author', '.author'))
            if author_elem:
                author = author_elem.get_text(strip=True)
            
            description = ""
            desc_elem = item.select_one(selectors.get('description', '.description'))
            if desc_elem:
                description = desc_elem.get_text(strip=True)
            
            cover_url = ""
            cover_elem = item.select_one(selectors.get('cover', 'img'))
            if cover_elem:
                cover_url = cover_elem.get('src', '') or cover_elem.get('data-src', '')
                if cover_url:
                    cover_url = urljoin(source['base_url'], cover_url)
            
            # Try to extract chapter count
            total_chapters = 0
            chapter_text = item.get_text()
            chapter_match = re.search(r'(\d+)\s*(?:chapters?|ch\.?)', chapter_text, re.I)
            if chapter_match:
                total_chapters = int(chapter_match.group(1))
            
            return {
                "title": title,
                "author": author or "Unknown",
                "url": url,
                "source_name": source['name'],
                "total_chapters": total_chapters,
                "description": description[:500] if description else "",
                "cover_url": cover_url
            }
            
        except Exception as e:
            logger.error(f"Error parsing search item: {e}")
            return None
    
    def get_content_info(self, content_url: str, source_name: str) -> Dict:
        """Get detailed information about content"""
        source = self.sources.get(source_name)
        if not source:
            raise ValueError(f"Unknown source: {source_name}")
        
        try:
            response = self.session.get(content_url, timeout=30)
            response.raise_for_status()
            
            soup = BeautifulSoup(response.text, 'html.parser')
            selectors = source.get('selectors', {})
            
            info = {
                'title': '',
                'author': 'Unknown',
                'description': '',
                'cover_url': '',
                'total_chapters': 0,
                'url': content_url,
                'source': source_name
            }
            
            # Extract title
            title_elem = soup.select_one(selectors.get('novel_title', 'h1'))
            if title_elem:
                info['title'] = title_elem.get_text(strip=True)
            
            # Extract author
            author_elem = soup.select_one(selectors.get('novel_author', '.author'))
            if author_elem:
                info['author'] = author_elem.get_text(strip=True)
            
            # Extract description
            desc_elems = soup.select(selectors.get('novel_description', '.description'))
            if desc_elems:
                info['description'] = ' '.join([elem.get_text(strip=True) for elem in desc_elems])
            
            # Extract cover - Special handling for NovelFire
            if source_name == "NovelFire":
                # Look for the book cover image specifically
                cover_elem = soup.select_one('.book-cover img')
                if not cover_elem:
                    # Try alternative selectors
                    cover_elem = soup.select_one('img[alt*="' + info['title'] + '"]')
                if not cover_elem:
                    # Try generic img in the book info area
                    cover_elem = soup.select_one('.book-info img')
                
                if cover_elem:
                    cover_url = cover_elem.get('src', '') or cover_elem.get('data-src', '')
                    if cover_url:
                        info['cover_url'] = urljoin(source['base_url'], cover_url)
            else:
                # Generic cover extraction
                cover_elem = soup.select_one(selectors.get('novel_cover', 'img'))
                if cover_elem:
                    cover_url = cover_elem.get('src', '') or cover_elem.get('data-src', '')
                    if cover_url:
                        info['cover_url'] = urljoin(source['base_url'], cover_url)
            
            # Extract chapter count
            info['total_chapters'] = self._extract_chapter_count(soup, source)
            
            return info
            
        except Exception as e:
            logger.error(f"Error getting content info: {e}")
            raise
    
    def _extract_chapter_count(self, soup, source: Dict) -> int:
        """Extract total chapter count from page"""
        # Try various methods to find chapter count
        
        # Method 1: Look for chapter count text
        patterns = [
            r'(\d+)\s*chapters?',
            r'chapters?\s*:\s*(\d+)',
            r'total\s*chapters?\s*:\s*(\d+)',
        ]
        
        page_text = soup.get_text()
        for pattern in patterns:
            match = re.search(pattern, page_text, re.I)
            if match:
                return int(match.group(1))
        
        # Method 2: Count chapter links
        chapter_selector = source.get('selectors', {}).get('chapter_list', '.chapter-list a')
        chapter_links = soup.select(chapter_selector)
        if chapter_links:
            return len(chapter_links)
        
        # Default fallback
        return 1000
    
    def download_content(self, content_url: str, source_name: str, output_dir: str,
                        content_type: str = "novel", start_chapter: int = 1,
                        end_chapter: int = -1, download_id: str = None,
                        content_name: str = None) -> bool:
        """Download content (novel or manga)"""
        try:
            source = self.sources.get(source_name)
            if not source:
                raise ValueError(f"Unknown source: {source_name}")
            
            # If content_name is provided, try to convert it to URL
            if content_name and not content_url.startswith('http'):
                content_url = self._name_to_url(content_name, source)
            
            # Get content info
            logger.info(f"Getting content info from: {content_url}")
            content_info = self.get_content_info(content_url, source_name)
            
            if not content_info['title']:
                logger.error("Could not extract content title")
                return False
            
            # Create download ID if not provided
            if not download_id:
                download_id = f"{source_name}_{self._title_to_slug(content_info['title'])}_{int(time.time())}"
            
            # Initialize download state
            self._update_download_state(download_id, content_info['title'], 
                                      0, content_info['total_chapters'], 
                                      0.0, "Starting", "", content_type)
            
            # Determine content type from source
            source_types = source.get('content_types', ['novel'])
            if 'manga' in source_types or 'manhwa' in source_types or 'manhua' in source_types:
                return self._download_manga(content_info, source, output_dir, 
                                          start_chapter, end_chapter, download_id)
            else:
                return self._download_novel(content_info, source, output_dir,
                                          start_chapter, end_chapter, download_id)
            
        except Exception as e:
            logger.error(f"Download error: {e}")
            if download_id:
                self._update_download_state(download_id, "", 0, 0, 0.0, 
                                          "Failed", str(e), content_type)
            return False
    
    def _name_to_url(self, name: str, source: Dict) -> str:
        """Convert content name to URL"""
        slug = self._title_to_slug(name)
        
        if source['name'] == 'NovelFire':
            return f"{source['base_url']}/book/{slug}"
        elif source['name'] == 'Comick':
            return f"{source['base_url']}/comic/{slug}"
        else:
            # Generic pattern
            return f"{source['base_url']}/content/{slug}"
    
    def _title_to_slug(self, title: str) -> str:
        """Convert title to URL slug"""
        slug = title.lower()
        slug = re.sub(r'[^\w\s-]', '', slug)
        slug = re.sub(r'[-\s]+', '-', slug)
        return slug.strip('-')
    
    def _update_novel_downloaded_count(self, novel_dir: str, downloaded_count: int):
        """Update the downloadedchapters count in Novels.json"""
        try:
            novels_json_path = os.path.join(os.path.dirname(novel_dir), 'Novels.json')
            if os.path.exists(novels_json_path):
                with open(novels_json_path, 'r', encoding='utf-8') as f:
                    novels_data = json.load(f)
            
                # Find the novel and update its downloaded count
                novel_name = os.path.basename(novel_dir)
                for novel in novels_data.get('novels', []):
                    if novel.get('name') == novel_name:
                        novel['downloadedchapters'] = downloaded_count
                        break
            
                # Save updated data
                with open(novels_json_path, 'w', encoding='utf-8') as f:
                    json.dump(novels_data, f, indent=4, ensure_ascii=False)
                
        except Exception as e:
            logger.error(f"Error updating downloaded chapter count: {e}")
    
    def _download_novel(self, content_info: Dict, source: Dict, output_dir: str,
                       start_chapter: int, end_chapter: int, download_id: str) -> bool:
        """Download novel chapters"""
        try:
            novel_name = content_info['title']
            novel_dir = os.path.join(output_dir, self._sanitize_filename(novel_name))
            chapters_dir = os.path.join(novel_dir, 'chapters')
        
            # Create directories
            logger.info(f"Creating directories: {novel_dir}")
            os.makedirs(chapters_dir, exist_ok=True)
        
            # Save metadata
            logger.info("Saving novel metadata...")
            self._save_novel_metadata(content_info, novel_dir)
        
            # Download cover
            if content_info.get('cover_url'):
                logger.info("Downloading cover image...")
                self._download_cover(content_info['cover_url'], novel_dir)
        
            # Determine chapter range
            total_chapters = content_info.get('total_chapters', 0)
            if total_chapters == 0:
                logger.error("No chapters found")
                return False
            
            if end_chapter == -1 or end_chapter > total_chapters:
                end_chapter = min(total_chapters, start_chapter + 999)  # Limit to 1000 chapters
        
            # Validate chapter range
            if start_chapter < 1:
                start_chapter = 1
            if end_chapter < start_chapter:
                logger.error(f"Invalid chapter range: {start_chapter} to {end_chapter}")
                return False
        
            logger.info(f"Downloading chapters {start_chapter} to {end_chapter} of {total_chapters} total")
        
            # Initialize counters
            downloaded_count = 0
            total_to_download = end_chapter - start_chapter + 1
        
            # Update initial state
            self._update_download_state(download_id, novel_name, 0, total_to_download, 
                                      0.0, "Starting", "", "novel")
        
            # Download chapters
            for chapter_num in range(start_chapter, end_chapter + 1):
                try:
                    # Check if should stop
                    if self._should_stop_download(download_id):
                        logger.info(f"Download stopped by user at chapter {chapter_num}")
                        self._update_download_state(download_id, novel_name, downloaded_count,
                                              total_to_download, 
                                              (downloaded_count / total_to_download) * 100,
                                              "Stopped", "", "novel")
                        return False
                
                    chapter_file = os.path.join(chapters_dir, f"chapter{chapter_num}.json")
                
                    # Skip if already exists
                    if os.path.exists(chapter_file):
                        logger.info(f"Chapter {chapter_num} already exists, skipping...")
                        downloaded_count += 1
                        progress = (downloaded_count / total_to_download) * 100
                    
                        # Still report progress for skipped chapters
                        print(f"Progress: {downloaded_count}/{total_to_download} ({progress:.1f}%) - Chapter {chapter_num} (skipped)", 
                            file=sys.stderr, flush=True)
                        continue
                
                    # Generate chapter URL
                    chapter_url = self._get_chapter_url(content_info['url'], chapter_num, source)
                    logger.info(f"Downloading from: {chapter_url}")
                
                    # Download chapter
                    chapter_data = self._download_novel_chapter(chapter_url, source, chapter_num)
                
                    if not chapter_data:
                        logger.error(f"Failed to download chapter {chapter_num}")
                        continue
                
                    # Validate chapter data
                    if 'content' not in chapter_data or not chapter_data['content']:
                        logger.error(f"Chapter {chapter_num} has no content")
                        continue
                
                    # Save chapter
                    with open(chapter_file, 'w', encoding='utf-8') as f:
                        json.dump(chapter_data, f, indent=2, ensure_ascii=False)
                
                    downloaded_count += 1
                    progress = (downloaded_count / total_to_download) * 100
                
                    # Update download state
                    self._update_download_state(download_id, novel_name, downloaded_count,
                                            total_to_download, progress, "Downloading", "", "novel")
                
                    # Output progress in the exact format C++ expects
                    chapter_title = chapter_data.get('title', f'Chapter {chapter_num}')
                    print(f"Progress: {downloaded_count}/{total_to_download} ({progress:.1f}%) - {chapter_title}", 
                        file=sys.stderr, flush=True)
                
                    logger.info(f"Successfully downloaded chapter {chapter_num}: {chapter_title}")
                
                    # Rate limiting
                    if downloaded_count % 10 == 0:
                        logger.info(f"Downloaded {downloaded_count} chapters, pausing for 5 seconds...")
                        time.sleep(5)
                    else:
                        time.sleep(1)
                
                except Exception as e:
                    logger.error(f"Error downloading chapter {chapter_num}: {str(e)}")
                    # Continue with next chapter instead of failing completely
                    continue
        
            # Final status update
            if downloaded_count == total_to_download:
                self._update_download_state(download_id, novel_name, downloaded_count,
                                        total_to_download, 100.0, "Complete", "", "novel")
                logger.info(f"Novel download complete: {novel_name} - Downloaded {downloaded_count} chapters")
            
                # Update the novel metadata with actual downloaded count
                self._update_novel_downloaded_count(novel_dir, downloaded_count)
            
                return True
            else:
                error_msg = f"Only downloaded {downloaded_count} of {total_to_download} chapters"
                self._update_download_state(download_id, novel_name, downloaded_count,
                                        total_to_download, 
                                        (downloaded_count / total_to_download) * 100,
                                        "Partial", error_msg, "novel")
                logger.warning(error_msg)
                return False
        
        except Exception as e:
            error_msg = f"Fatal error in download: {str(e)}"
            logger.error(error_msg)
            import traceback
            traceback.print_exc(file=sys.stderr)
        
            # Update state with error
            if 'novel_name' in locals():
                self._update_download_state(download_id, novel_name, 0, 0, 0.0, 
                                      "Failed", error_msg, "novel")
            else:
                self._update_download_state(download_id, "Unknown", 0, 0, 0.0, 
                                      "Failed", error_msg, "novel")
        
            return False
    
    def _download_manga(self, content_info: Dict, source: Dict, output_dir: str,
                       start_chapter: int, end_chapter: int, download_id: str) -> bool:
        """Download manga chapters"""
        manga_name = content_info['title']
        manga_dir = os.path.join("Manga", self._sanitize_filename(manga_name))
        os.makedirs(manga_dir, exist_ok=True)
        
        # Save metadata
        self._save_manga_metadata(content_info, manga_dir)
        
        # Download cover
        if content_info['cover_url']:
            self._download_cover(content_info['cover_url'], manga_dir)
        
        # Get chapter providers
        providers = self._get_chapter_providers(content_info['url'], source)
        if not providers:
            logger.error("No chapter providers found")
            return False
        
        # Select best provider (prefer English)
        selected_provider = self._select_best_provider(providers)
        
        # Get chapter URLs
        chapter_urls = self._get_manga_chapter_urls(selected_provider, source, start_chapter, end_chapter)
        
        if not chapter_urls:
            logger.error("No chapter URLs found")
            return False
        
        total_chapters = len(chapter_urls)
        downloaded_count = 0
        
        for chapter_num, chapter_url in chapter_urls:
            if self._should_stop_download(download_id):
                return False
            
            chapter_dir = os.path.join(manga_dir, f"Chapter_{chapter_num:03d}")
            
            # Skip if already downloaded
            if self._chapter_already_downloaded(chapter_dir):
                downloaded_count += 1
                continue
            
            os.makedirs(chapter_dir, exist_ok=True)
            
            try:
                # Download chapter images
                images = self._get_chapter_images(chapter_url, source)
                if not images:
                    logger.error(f"No images found for chapter {chapter_num}")
                    continue
                
                # Download each image
                for i, image_url in enumerate(images):
                    if self._should_stop_download(download_id):
                        return False
                    
                    ext = self._get_image_extension(image_url)
                    image_path = os.path.join(chapter_dir, f"page_{i:03d}{ext}")
                    
                    if not os.path.exists(image_path):
                        self._download_image(image_url, image_path)
                
                # Save chapter metadata
                chapter_meta = {
                    'chapter_number': chapter_num,
                    'title': f"Chapter {chapter_num}",
                    'page_count': len(images),
                    'provider': selected_provider.name,
                    'language': selected_provider.language,
                    'download_date': datetime.now().isoformat()
                }
                
                with open(os.path.join(chapter_dir, "metadata.json"), 'w', encoding='utf-8') as f:
                    json.dump(chapter_meta, f, indent=2, ensure_ascii=False)
                
                downloaded_count += 1
                progress = (downloaded_count / total_chapters) * 100
                
                self._update_download_state(download_id, manga_name, downloaded_count,
                                          total_chapters, progress, "Downloading", "", ContentType.MANGA.value)
                
                logger.info(f"Progress: {downloaded_count}/{total_chapters} ({progress:.1f}%) - Chapter {chapter_num}")
                
                # Rate limiting
                time.sleep(2)
                
            except Exception as e:
                logger.error(f"Error downloading chapter {chapter_num}: {e}")
                continue
        
        # Mark as complete
        self._update_download_state(download_id, manga_name, downloaded_count,
                                  total_chapters, 100.0, "Complete", "", ContentType.MANGA.value)
        
        logger.info(f"Manga download complete: {manga_name}")
        return True
    
    def _get_chapter_url(self, novel_url: str, chapter_num: int, source: Dict) -> str:
        """Generate chapter URL based on novel URL pattern"""
        if source['name'] == 'NovelFire':
            # Extract novel slug from URL
            slug_match = re.search(r'/book/([^/]+)', novel_url)
            if slug_match:
                slug = slug_match.group(1)
                return f"{source['base_url']}/book/{slug}/chapter-{chapter_num}"
        
        # Generic pattern
        return f"{novel_url}/chapter-{chapter_num}"
    
    def _download_novel_chapter(self, chapter_url: str, source: Dict, chapter_num: int) -> Dict:
        """Download a single novel chapter"""
        try:
            response = self.session.get(chapter_url, timeout=30)
            response.raise_for_status()
            
            soup = BeautifulSoup(response.text, 'html.parser')
            
            # Extract title
            title_selector = source.get('selectors', {}).get('chapter_title', '')
            title = f"Chapter {chapter_num}"
            if title_selector:
                title_elem = soup.select_one(title_selector)
                if title_elem:
                    title = title_elem.get_text(strip=True)
            
            # Extract content
            content_selector = source.get('selectors', {}).get('chapter_content', '')
            if not content_selector:
                raise ValueError(f"No content selector defined for {source['name']}")
            
            content_elem = soup.select_one(content_selector)
            if not content_elem:
                raise ValueError(f"Could not find chapter content")
            
            # Clean content
            content = self._clean_chapter_content(content_elem, source)
            
            if not content or len(content.strip()) < 50:
                raise ValueError(f"Chapter content too short or empty")
            
            return {
                'chapterNumber': chapter_num,
                'title': title,
                'content': content
            }
            
        except Exception as e:
            logger.error(f"Error downloading chapter {chapter_num}: {e}")
            raise
    
    def _clean_chapter_content(self, content_elem, source: Dict) -> str:
        """Clean and format chapter content"""
        # Clone element
        content_copy = BeautifulSoup(str(content_elem), 'html.parser')
        
        # Remove unwanted elements
        remove_selectors = source.get('selectors', {}).get('remove_selectors', [])
        for selector in remove_selectors:
            for elem in content_copy.select(selector):
                elem.decompose()
        
        # Convert to text with paragraph breaks
        content = ""
        for elem in content_copy.find_all(['p', 'div'], recursive=True):
            text = elem.get_text(strip=True)
            if text and len(text) > 10:
                content += f"{text}\n\n"
        
        # Fallback if no paragraphs found
        if not content.strip():
            content = content_copy.get_text(separator='\n\n', strip=True)
        
        return content.strip()
    
    def _get_chapter_providers(self, content_url: str, source: Dict) -> List[ChapterProvider]:
        """Get available chapter providers/translations"""
        # Simplified - return a default provider
        return [ChapterProvider(
            name="Default",
            language="en",
            url=content_url,
            upload_date="",
            chapter_title="Chapter 1"
        )]
    
    def _select_best_provider(self, providers: List[ChapterProvider]) -> ChapterProvider:
        """Select the best provider, preferring English"""
        for provider in providers:
            if provider.language == "en":
                return provider
        return providers[0] if providers else None
    
    def _get_manga_chapter_urls(self, provider: ChapterProvider, source: Dict, 
                               start_chapter: int, end_chapter: int) -> List[Tuple[int, str]]:
        """Get manga chapter URLs"""
        chapter_urls = []
        base_url = provider.url
        
        # Generate chapter URLs based on pattern
        for i in range(start_chapter, end_chapter + 1):
            if source['name'] == 'Comick':
                chapter_url = f"{base_url}/chapter-{i}"
            else:
                chapter_url = f"{base_url}/chapter-{i}"
            
            chapter_urls.append((i, chapter_url))
        
        return chapter_urls
    
    def _get_chapter_images(self, chapter_url: str, source: Dict) -> List[str]:
        """Get list of image URLs for a chapter"""
        try:
            response = self.session.get(chapter_url, timeout=30)
            response.raise_for_status()
            
            soup = BeautifulSoup(response.text, 'html.parser')
            
            # Extract images
            image_selector = source.get('selectors', {}).get('chapter_images', 'img')
            image_elements = soup.select(image_selector)
            
            images = []
            for img in image_elements:
                src = img.get('src') or img.get('data-src') or img.get('data-lazy-src')
                if src:
                    if not src.startswith('http'):
                        src = urljoin(source['base_url'], src)
                    images.append(src)
            
            return images
            
        except Exception as e:
            logger.error(f"Error getting chapter images: {e}")
            return []
    
    def _download_image(self, image_url: str, output_path: str, max_retries: int = 3):
        """Download image with retry logic"""
        for attempt in range(max_retries):
            try:
                response = self.session.get(image_url, timeout=30, stream=True)
                response.raise_for_status()
                
                with open(output_path, 'wb') as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        if chunk:
                            f.write(chunk)
                
                return True
                
            except Exception as e:
                if attempt < max_retries - 1:
                    time.sleep(2 ** attempt)
                else:
                    logger.error(f"Failed to download image {image_url}: {e}")
        
        return False
    
    def _download_cover(self, cover_url: str, output_dir: str):
        """Download cover image"""
        try:
            response = self.session.get(cover_url, timeout=30, stream=True)
            response.raise_for_status()
            
            # Determine extension
            content_type = response.headers.get('content-type', '').lower()
            ext = '.jpg'
            if 'png' in content_type:
               ext = '.png'
            elif 'gif' in content_type:
               ext = '.gif'
            elif 'webp' in content_type:
               ext = '.webp'
           
            cover_path = os.path.join(output_dir, f'cover{ext}')
           
            with open(cover_path, 'wb') as f:
               for chunk in response.iter_content(chunk_size=8192):
                   if chunk:
                       f.write(chunk)
           
            logger.info(f"Downloaded cover to {cover_path}")
           
        except Exception as e:
           logger.error(f"Error downloading cover: {e}")
   
    def _save_novel_metadata(self, content_info: Dict, novel_dir: str):
       """Save novel metadata"""
       try:
           # Update Novels.json for C++ app compatibility
           novels_json_path = os.path.join(os.path.dirname(novel_dir), 'Novels.json')
           novels_data = {"novels": []}
           
           # Load existing
           if os.path.exists(novels_json_path):
               try:
                   with open(novels_json_path, 'r', encoding='utf-8') as f:
                       novels_data = json.load(f)
               except:
                   pass
           
           # Create novel info
           novel_info = {
               'name': content_info['title'],
               'authorname': content_info.get('author', 'Unknown'),
               'coverpath': os.path.join(novel_dir, 'cover.jpg'),
               'synopsis': content_info.get('description', ''),
               'totalchapters': content_info.get('total_chapters', 0),
               'downloadedchapters': 0,  # Will be updated by C++ app
               'progress': {
                   'readchapters': 0,
                   'progresspercentage': 0.0
               }
           }
           
           # Update or add
           existing_idx = None
           for i, novel in enumerate(novels_data.get("novels", [])):
               if novel.get('name') == content_info['title']:
                   existing_idx = i
                   break
           
           if existing_idx is not None:
               novels_data["novels"][existing_idx] = novel_info
           else:
               novels_data["novels"].append(novel_info)
           
           # Save
           with open(novels_json_path, 'w', encoding='utf-8') as f:
               json.dump(novels_data, f, indent=4, ensure_ascii=False)
           
           # Also save individual metadata
           metadata = {
               'title': content_info['title'],
               'author': content_info.get('author', 'Unknown'),
               'description': content_info.get('description', ''),
               'source': content_info['source'],
               'url': content_info['url'],
               'cover_url': content_info.get('cover_url', ''),
               'download_date': datetime.now().isoformat(),
               'content_type': 'novel'
           }
           
           with open(os.path.join(novel_dir, 'metadata.json'), 'w', encoding='utf-8') as f:
               json.dump(metadata, f, indent=2, ensure_ascii=False)
               
       except Exception as e:
           logger.error(f"Error saving novel metadata: {e}")
   
    def _save_manga_metadata(self, content_info: Dict, manga_dir: str):
       """Save manga metadata"""
       try:
           metadata = {
               'title': content_info['title'],
               'author': content_info.get('author', 'Unknown'),
               'description': content_info.get('description', ''),
               'source': content_info['source'],
               'url': content_info['url'],
               'cover_url': content_info.get('cover_url', ''),
               'download_date': datetime.now().isoformat(),
               'content_type': 'manga'
           }
           
           with open(os.path.join(manga_dir, 'metadata.json'), 'w', encoding='utf-8') as f:
               json.dump(metadata, f, indent=2, ensure_ascii=False)
               
       except Exception as e:
           logger.error(f"Error saving manga metadata: {e}")
   
    def _get_image_extension(self, image_url: str) -> str:
       """Get image extension from URL"""
       parsed = urlparse(image_url)
       path = parsed.path.lower()
       
       for ext in ['.jpg', '.jpeg', '.png', '.gif', '.webp']:
           if path.endswith(ext):
               return ext
       
       return '.jpg'  # Default
   
    def _chapter_already_downloaded(self, chapter_dir: str) -> bool:
       """Check if chapter is already completely downloaded"""
       if not os.path.exists(chapter_dir):
           return False
       
       metadata_file = os.path.join(chapter_dir, "metadata.json")
       if not os.path.exists(metadata_file):
           return False
       
       try:
           with open(metadata_file, 'r', encoding='utf-8') as f:
               metadata = json.load(f)
           
           expected_pages = metadata.get('page_count', 0)
           if expected_pages == 0:
               return False
           
           # Count actual image files
           image_files = [f for f in os.listdir(chapter_dir) 
                         if f.startswith('page_') and f.endswith(('.jpg', '.png', '.gif', '.webp'))]
           
           return len(image_files) >= expected_pages
           
       except Exception:
           return False
   
    def _sanitize_filename(self, filename: str) -> str:
       """Sanitize filename for filesystem"""
       # Remove invalid characters
       invalid_chars = '<>:"/\\|?*'
       for char in invalid_chars:
           filename = filename.replace(char, '_')
       
       # Remove leading/trailing whitespace and dots
       filename = filename.strip(' .')
       
       # Limit length
       if len(filename) > 100:
           filename = filename[:100]
       
       return filename
   
    def _should_stop_download(self, download_id: str) -> bool:
       """Check if download should be stopped"""
       # Check for stop signal files
       stop_file = f"downloads/.stop_{download_id}"
       pause_file = f"downloads/.pause_{download_id}"
       cancel_file = f"downloads/.cancel_{download_id}"
       
       if os.path.exists(stop_file) or os.path.exists(cancel_file):
           return True
       
       if os.path.exists(pause_file):
           # Wait until resumed
           while os.path.exists(pause_file):
               time.sleep(1)
       
       return False
   
    def _update_download_state(self, download_id: str, content_name: str, 
                            current_chapter: int, total_chapters: int, 
                            progress: float, status: str, error: str = "",
                            content_type: str = "novel"):
       """Update download state"""
       state = {
           'id': download_id,
           'contentName': content_name,
           'contentType': content_type,
           'currentChapter': current_chapter,
           'totalChapters': total_chapters,
           'progress': progress,
           'status': status,
           'lastError': error,
           'lastUpdate': datetime.now().isoformat(),
           'isPaused': status == "Paused",
           'isComplete': status == "Complete"
       }
       
       self._save_download_state(download_id, state)
   
    def _save_download_state(self, download_id: str, state: Dict):
       """Save download state to file"""
       try:
           os.makedirs("downloads", exist_ok=True)
           state_file = f"downloads/state_{download_id}.json"
           
           with open(state_file, 'w', encoding='utf-8') as f:
               json.dump(state, f, indent=2, ensure_ascii=False)
               
       except Exception as e:
           logger.error(f"Error saving download state: {e}")
   
    def get_download_status(self, download_id: str) -> Optional[Dict]:
       """Get download status"""
       try:
           state_file = f"downloads/state_{download_id}.json"
           if os.path.exists(state_file):
               with open(state_file, 'r', encoding='utf-8') as f:
                   return json.load(f)
       except Exception:
           pass
       return None
   
    def pause_download(self, download_id: str):
       """Pause a download"""
       os.makedirs("downloads", exist_ok=True)
       pause_file = f"downloads/.pause_{download_id}"
       with open(pause_file, 'w') as f:
           f.write("PAUSE")
   
    def resume_download(self, download_id: str):
       """Resume a download"""
       pause_file = f"downloads/.pause_{download_id}"
       if os.path.exists(pause_file):
           os.remove(pause_file)
   
    def cancel_download(self, download_id: str):
       """Cancel a download"""
       os.makedirs("downloads", exist_ok=True)
       cancel_file = f"downloads/.cancel_{download_id}"
       with open(cancel_file, 'w') as f:
           f.write("CANCEL")
   
    def list_downloads(self) -> List[Dict]:
       """List all downloads"""
       downloads = []
       
       try:
           if os.path.exists("downloads"):
               for file in os.listdir("downloads"):
                   if file.startswith("state_") and file.endswith(".json"):
                       download_id = file[6:-5]  # Remove "state_" and ".json"
                       status = self.get_download_status(download_id)
                       if status:
                           downloads.append(status)
       except Exception as e:
           logger.error(f"Error listing downloads: {e}")
       
       return downloads


def main():
   parser = argparse.ArgumentParser(description='Universal Content Download Manager')
   parser.add_argument('action', choices=['search', 'download', 'info', 'pause', 'resume', 'cancel', 'status', 'list'])
   parser.add_argument('--query', help='Search query')
   parser.add_argument('--url', help='Content URL')
   parser.add_argument('--name', help='Content name (will be converted to URL)')
   parser.add_argument('--source', help='Source name')
   parser.add_argument('--output', help='Output directory', default='Novels')
   parser.add_argument('--start', type=int, default=1, help='Start chapter')
   parser.add_argument('--end', type=int, default=-1, help='End chapter (-1 for all)')
   parser.add_argument('--config', help='Source config file', default='sources.json')
   parser.add_argument('--content-type', help='Content type filter', 
                      choices=['all', 'novel', 'manga', 'manhwa', 'manhua'], default='all')
   parser.add_argument('--language', help='Language filter', default='')
   parser.add_argument('--include-adult', action='store_true', help='Include adult content')
   parser.add_argument('--max-results', type=int, default=2, help='Max results per source')
   parser.add_argument('--download-id', help='Download ID')
   
   args = parser.parse_args()
   
   try:
       downloader = UniversalDownloader(args.config)
       
       if args.action == 'search':
           if not args.query:
               logger.error("Search query is required")
               print(json.dumps([]))
               return 1
           
           results = downloader.search_content(
               query=args.query,
               content_type=args.content_type,
               language=args.language,
               include_adult=args.include_adult,
               max_results_per_source=args.max_results
           )
           
           # Convert to list of dicts for JSON output
           output = []
           for result in results:
               if isinstance(result, dict):
                   output.append(result)
               else:
                   # Convert SearchResult to dict if needed
                   output.append({
                       "title": result.title,
                       "author": result.author,
                       "url": result.url,
                       "source_name": result.source_name,
                       "total_chapters": result.total_chapters,
                       "description": result.description,
                       "cover_url": result.cover_url
                   })
           
           print(json.dumps(output, ensure_ascii=True))
       
       elif args.action == 'download':
           if not args.source:
               logger.error("Source is required for download")
               return 1
           
           # Determine what was provided (URL or name)
           content_url = args.url if args.url else ""
           content_name = args.name if args.name else ""
           
           if not content_url and not content_name:
               logger.error("Either URL or name is required for download")
               return 1
           
           download_id = args.download_id or f"download_{int(time.time())}"
           
           success = downloader.download_content(
               content_url=content_url,
               source_name=args.source,
               output_dir=args.output,
               content_type=args.content_type,
               start_chapter=args.start,
               end_chapter=args.end,
               download_id=download_id,
               content_name=content_name
           )
           
           if success:
               logger.info(f"Download completed successfully. ID: {download_id}")
           else:
               logger.error(f"Download failed. ID: {download_id}")
               return 1
       
       elif args.action == 'info':
           if not args.url or not args.source:
               logger.error("URL and source are required for info")
               return 1
           
           info = downloader.get_content_info(args.url, args.source)
           print(json.dumps(info, ensure_ascii=False))
       
       elif args.action == 'pause':
           if not args.download_id:
               logger.error("Download ID is required for pause")
               return 1
           
           downloader.pause_download(args.download_id)
           logger.info(f"Download {args.download_id} paused")
       
       elif args.action == 'resume':
           if not args.download_id:
               logger.error("Download ID is required for resume")
               return 1
           
           downloader.resume_download(args.download_id)
           logger.info(f"Download {args.download_id} resumed")
       
       elif args.action == 'cancel':
           if not args.download_id:
               logger.error("Download ID is required for cancel")
               return 1
           
           downloader.cancel_download(args.download_id)
           logger.info(f"Download {args.download_id} cancelled")
       
       elif args.action == 'status':
           if not args.download_id:
               logger.error("Download ID is required for status")
               return 1
           
           status = downloader.get_download_status(args.download_id)
           if status:
               print(json.dumps(status, ensure_ascii=False))
           else:
               logger.error(f"Download {args.download_id} not found")
               return 1
       
       elif args.action == 'list':
           downloads = downloader.list_downloads()
           print(json.dumps(downloads, ensure_ascii=False))
       
       return 0
       
   except KeyboardInterrupt:
       logger.info("\nOperation cancelled by user")
       return 1
   except Exception as e:
       logger.error(f"Error: {e}")
       import traceback
       traceback.print_exc()
       return 1


if __name__ == "__main__":
   sys.exit(main())